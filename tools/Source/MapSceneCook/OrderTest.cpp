#include "SceneOrder.h"
#include "SceneCook.h"
#include "MapSourceReader.h"
#include "MapTextureCache.h"
#include <fstream>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
}
void test_scene_order(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);const auto source=root/"spatial.gltf",binary=root/"spatial.bin",package=root/"package"/"scene.json";
  std::vector<float> positions,normals,uv;std::vector<std::uint32_t> indices;
  for(unsigned triangle=0;triangle<256;++triangle)for(unsigned corner=0;corner<3;++corner) {
    const float x=float(triangle%16)*100+(corner==1?1.f:0.f),y=float(triangle/16)*.01f+(corner==2?1.f:0.f);
    positions.insert(positions.end(),{x,y,0});normals.insert(normals.end(),{0,0,1});
    uv.insert(uv.end(),{corner==1?1.f:0.f,corner==2?1.f:0.f});indices.push_back(unsigned(indices.size()));
  }
  {std::ofstream file(binary,std::ios::binary);for(const auto* values:{&positions,&normals,&uv})
      file.write(reinterpret_cast<const char*>(values->data()),std::streamsize(values->size()*4));
    file.write(reinterpret_cast<const char*>(indices.data()),std::streamsize(indices.size()*4));}
  {
    std::ofstream file(source);file<<R"({"asset":{"version":"2.0"},"buffers":[{"uri":"spatial.bin","byteLength":27648}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":9216},{"buffer":0,"byteOffset":9216,"byteLength":9216},
{"buffer":0,"byteOffset":18432,"byteLength":6144},{"buffer":0,"byteOffset":24576,"byteLength":3072}],
"accessors":[{"bufferView":0,"componentType":5126,"count":768,"type":"VEC3","min":[0,0,0],"max":[1501,1.15,0]},
{"bufferView":1,"componentType":5126,"count":768,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":768,"type":"VEC2"},
{"bufferView":3,"componentType":5125,"count":768,"type":"SCALAR"}],
"materials":[{"name":"Authored opaque","pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.8,1]}},
{"name":"Authored blend","alphaMode":"BLEND","pbrMetallicRoughness":{"baseColorFactor":[0.8,0.4,0.2,0.5]}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0},
{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":1}]}],
"nodes":[{"name":"first","mesh":0,"translation":[10,0,0]},{"name":"mirrored","mesh":0,"translation":[-10,0,0],"scale":[-1,2,1]}],
"scenes":[{"nodes":[0,1]}],"scene":0})";
  }
  SceneCatalog catalog;std::string error;const auto success=[&](bool value){if(!value)throw std::runtime_error(error);};
  success(import_scene_catalog(source,catalog,error));const auto identity=catalog.source_hash;catalog.part_triangles=16;catalog.parts.clear();
  for(unsigned id=0;id<catalog.primitives.size();++id) {
    auto& primitive=catalog.primitives[id];primitive.first_part=unsigned(catalog.parts.size());primitive.part_count=16;
    for(unsigned part=0;part<16;++part) {ScenePart value;value.primitive=id;value.first_triangle=part*16;value.triangle_count=16;value.bounds=primitive.bounds;catalog.parts.push_back(value);}
  }
  success(write_scene_catalog(package,catalog,error));require(cook_scene_parts(package,0,1),"source-order fixture cook failed");
  success(read_scene_catalog(package,catalog,error));const auto old_hash=catalog.parts[0].hash;
  success(prepare_scene_spatial_order(package,0,UINT64_MAX,error));success(read_scene_catalog(package,catalog,error));
  success(verify_scene_orders(package,catalog,error));
  require(catalog.source_hash==identity && catalog.instances.size()==2 && catalog.instanced_triangles==1024 &&
      catalog.instances[1].transform[0]==-1 && catalog.instances[1].transform[5]==2,"spatial order changed source nodes or coverage");
  MapSourceReader reader;success(reader.open(source,root/"scratch",error));
  for(const auto& part:catalog.parts) {
    require(part.geometry.empty() && part.bounds_prepared && part.bounds[3]-part.bounds[0]==1,"spatial order retained stale cook or broad part bounds");
    MapModel model;success(load_scene_part(reader,package,catalog,part,model,error));
    require(model.indices.size()==48 && model.primitives.size()==1,"ordered source gather changed triangle coverage");
    require(model.primitives[0].material.alpha_mode==catalog.primitives[part.primitive].surface.alpha_mode,"ordered source gather changed material");
    for(const auto& vertex:model.vertices)require(vertex.normal[2]==1 && vertex.uv[0]>=0 && vertex.uv[0]<=1 && vertex.uv[1]>=0 && vertex.uv[1]<=1,
        "ordered source gather changed authored attributes");
  }
  require(cook_scene_parts(package,0,UINT64_MAX),"ordered scene cook failed");success(read_scene_catalog(package,catalog,error));
  require(catalog.parts[0].hash!=old_hash,"ordered cook reused the source-order identity");
  for(const auto& part:catalog.parts)require(!part.geometry.empty(),"ordered complete scene omitted a primitive");
  const auto stamp=std::filesystem::last_write_time(package);success(prepare_scene_spatial_order(package,0,UINT64_MAX,error));
  require(std::filesystem::last_write_time(package)==stamp,"valid spatial order resume rewrote catalog");
  require(import_scene_catalog(source,catalog,error) && catalog.source_hash==identity,"spatial preparation changed source files");
  std::puts("scene_order_tests passed=1 source_coverage=1 shared_instances=1 materials=1 authored_attributes=1 spatial_bounds=1 distinct_cook_identity=1 source_unchanged=1 resume=1");
}
