#include "SceneCook.h"
#include "SceneCatalog.h"
#include "GeometryCache.h"
#include "MapTextureCache.h"
#include <fstream>
#include <cstdio>
#include <stdexcept>

void test_geometry_transform();
void test_scene_order(const std::filesystem::path&);
void test_scene_preparation(const std::filesystem::path&,const std::filesystem::path&);

bool test_scene_catalog(const std::filesystem::path& root) {
  using namespace octaryn::client::rendering;
  using namespace octaryn::client::rendering::virtual_geometry;
  const auto require=[](bool value,const std::string& error){if(!value)throw std::runtime_error(error);};
  test_geometry_transform();
  std::filesystem::create_directories(root);const auto source=root/"instances.gltf",buffer=root/"instances.bin";
  {
    const float positions[9]={0,0,0,1,0,0,0,1,0};const std::uint16_t indices[3]={0,1,2};
    std::ofstream file(buffer,std::ios::binary);file.write(reinterpret_cast<const char*>(positions),sizeof(positions));
    file.write(reinterpret_cast<const char*>(indices),sizeof(indices));
  }
  {
    std::ofstream file(source);
    file<<R"({"asset":{"version":"2.0"},"buffers":[{"uri":"instances.bin","byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"materials":[{"name":"Authored blue","doubleSided":true,"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.8,1],"metallicFactor":0.3,"roughnessFactor":0.7}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
"nodes":[{"name":"first","mesh":0,"translation":[10,0,0]},{"name":"mirrored","mesh":0,"translation":[-10,0,0],"scale":[-1,2,1]}],
"scenes":[{"nodes":[0,1]}],"scene":0})";
  }
  std::string error;const auto source_hash=map_texture_file_digest(source,error),buffer_hash=map_texture_file_digest(buffer,error);
  SceneCatalog catalog;
  require(import_scene_catalog(source,catalog,error),error);
  require(catalog.mesh_count==1 && catalog.primitives.size()==1 && catalog.parts.size()==1 && catalog.instances.size()==2 &&
      catalog.unique_triangles==1 && catalog.instanced_triangles==2,"catalog flattened, lost or duplicated instances");
  require(catalog.instances[0].transform[12]==10 && catalog.instances[1].transform[0]==-1 &&
      catalog.instances[1].transform[5]==2 && catalog.instances[1].bounds[0]==-11,"catalog changed authored transforms");
  require(catalog.primitives[0].surface.base_color[2]==.8f && catalog.primitives[0].surface.roughness==.7f,"catalog lost authored material");
  require(!write_scene_catalog(source,catalog,error) && !write_scene_catalog(buffer,catalog,error),"catalog writer accepted a source resource as its output");
  auto broken=catalog;broken.parts[0].first_triangle=1;
  require(!validate_scene_catalog(broken,error),"catalog accepted missing triangle coverage");
  broken=catalog;broken.instances.pop_back();
  require(!validate_scene_catalog(broken,error),"catalog accepted missing source instance");
  broken=catalog;broken.source_hash[0]=broken.source_hash[0]=='0'?'1':'0';
  require(!validate_scene_catalog(broken,error),"catalog accepted a mismatched source identity");
  broken=catalog;broken.resources.push_back(broken.resources.front());
  require(!validate_scene_catalog(broken,error),"catalog accepted a duplicated source resource");
  const auto package=root/"package"/"scene.json";
  require(write_scene_catalog(package,catalog,error),error);
  require(cook_scene_parts(package,0,UINT64_MAX),"instance fixture cook failed");
  require(read_scene_catalog(package,catalog,error),error);
  require(catalog.instances.size()==2 && !catalog.parts[0].geometry.empty() && catalog.parts[0].bounds_prepared,"cooked catalog lost source coverage or exact bounds");
  broken=catalog;broken.parts[0].root_pages=0;
  require(!validate_scene_catalog(broken,error),"catalog accepted cooked geometry without resident roots");
  const auto geometry=package.parent_path()/catalog.parts[0].geometry;GeometryAsset asset;
  require(read_geometry_cache(geometry,catalog.parts[0].hash,asset,error),error);
  require(asset.space==GeometrySpace::Object && asset.source_triangles==1 && asset.material_count==1,"cooked fixture is not shared object-space geometry");
  require(catalog.primitives[0].position_only && (asset.clusters[0].flags&geometry_position_only)!=0 &&
      asset.clusters[0].triangle_offset==asset.clusters[0].vertex_offset+asset.clusters[0].vertex_count*12,
      "POSITION-only source lost its generated-flat compact representation");
  const auto modified=std::filesystem::last_write_time(geometry);
  catalog.parts[0].bounds_prepared=false;catalog.parts[0].bounds={-100,-100,-100,100,100,100};
  require(write_scene_catalog(package,catalog,error),error);
  require(cook_scene_parts(package,0,UINT64_MAX) && std::filesystem::last_write_time(geometry)==modified,"scene resume rewrote valid geometry");
  require(read_scene_catalog(package,catalog,error) && catalog.parts[0].bounds_prepared &&
      catalog.parts[0].bounds==std::array<float,6>{0,0,0,1,1,0},"scene resume retained primitive-wide stale part bounds");
  require(map_texture_file_digest(source,error)==source_hash && map_texture_file_digest(buffer,error)==buffer_hash,"scene cook modified source files");
  const auto blend_source=root/"blend.gltf",blend_package=root/"blend-package"/"scene.json";
  {
    std::ifstream file(source);std::string text((std::istreambuf_iterator<char>(file)),{});
    const auto marker=text.find("\"doubleSided\":true");require(marker!=std::string::npos,"fixture material missing");
    text.insert(marker,"\"alphaMode\":\"BLEND\",");std::ofstream blend(blend_source);blend<<text;
  }
  require(import_scene_catalog(blend_source,catalog,error) && write_scene_catalog(blend_package,catalog,error),error);
  require(cook_scene_parts(blend_package,0,UINT64_MAX) && read_scene_catalog(blend_package,catalog,error),error);
  require(catalog.primitives[0].surface.alpha_mode==MapAlphaMode::Blend,"catalog discarded transparent material");
  require(read_geometry_cache(blend_package.parent_path()/catalog.parts[0].geometry,catalog.parts[0].hash,asset,error),error);
  require(asset.source_triangles==1 && (asset.clusters[0].flags&3)==2,"transparent source triangles were not cooked");
  test_scene_preparation(root,source);
  test_scene_order(root/"spatial-order");
  std::puts("scene_catalog_tests passed=1 instance_preservation=1 transforms=1 materials=1 coverage=1 object_space=1 resume=1 source_unchanged=1");
  return true;
}
