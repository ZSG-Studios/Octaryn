#include "ScenePreparationWorld.h"
#include "SceneCatalog.h"
#include "SceneHierarchy.h"
#include "ResourceDigest.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
void source(const std::filesystem::path& root) {
  std::vector<float> vertices;std::vector<unsigned> indices;
  for(unsigned tile=0;tile<257;++tile) {
    const auto first=unsigned(vertices.size()/3);const auto x=float(tile*16);
    vertices.insert(vertices.end(),{x,0,0,x+4,0,0,x,0,4,x+4,0,4});
    indices.insert(indices.end(),{first,first+2,first+1,first+1,first+2,first+3});
  }
  const auto vertex_bytes=vertices.size()*sizeof(float),index_bytes=indices.size()*sizeof(unsigned);
  std::ofstream buffer(root/"floor.bin",std::ios::binary);
  buffer.write(reinterpret_cast<const char*>(vertices.data()),std::streamsize(vertex_bytes));
  buffer.write(reinterpret_cast<const char*>(indices.data()),std::streamsize(index_bytes));buffer.close();
  std::ofstream gltf(root/"floor.gltf");
  gltf<<"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"floor.bin\",\"byteLength\":"<<vertex_bytes+index_bytes
      <<"}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":"<<vertex_bytes<<"},{\"buffer\":0,\"byteOffset\":"<<vertex_bytes
      <<",\"byteLength\":"<<index_bytes<<"}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":"<<vertices.size()/3
      <<",\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[4100,0,4]},{\"bufferView\":1,\"componentType\":5125,\"count\":"<<indices.size()
      <<R"(,"type":"SCALAR"}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.8,1],"roughnessFactor":0.7}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
"nodes":[{"mesh":0},{"mesh":0,"translation":[10000,0,0],"scale":[-1,2,1]}],"scenes":[{"nodes":[0,1]}],"scene":0})";
}
}
int test_scene_world_preparation(const std::filesystem::path& root) {
  using namespace octaryn::client::rendering::virtual_geometry;
  std::filesystem::create_directories(root);source(root);
  SceneCatalog original;std::string error;require(import_scene_catalog(root/"floor.gltf",original,error),error);
  original.part_triangles=2;original.parts.clear();auto& primitive=original.primitives.front();
  primitive.first_part=0;primitive.part_count=257;
  for(unsigned part=0;part<257;++part) {
    ScenePart p;p.primitive=0;p.first_triangle=part*2;p.triangle_count=2;p.bounds=primitive.bounds;original.parts.push_back(p);
  }
  ScenePreparationRequest request;request.source=root/"floor.gltf";request.catalog=root/"cache"/"scene.json";
  request.actor=request.camera={2,2,2};require(write_scene_catalog(request.catalog,original,error),error);
  const auto canonical=octaryn::content::resource_file_digest(request.catalog,error);
  const auto source_hash=octaryn::content::resource_file_digest(root/"floor.bin",error);
  ScenePreparationWorldResult result;std::atomic_bool cancel{};
  const auto notify=[&](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Geometry && p.completed==1)cancel=true;
  };
  require(!prepare_scene_world(request,result,error,&cancel,notify) && result.scene.canceled && !result.hierarchy_ready &&
      !result.scene.neighborhood_ready,"canceled large world was published as ready");
  SceneHierarchy hierarchy;
  require(read_scene_hierarchy(request.catalog.parent_path()/"hierarchy"/"scene.json",hierarchy,error) && !hierarchy.complete,
      "canceled world exposed a complete forest");
  require(!std::filesystem::exists(request.catalog.parent_path()/"world-scene.json"),"partial hierarchy published collision catalog");
  cancel=false;bool layout_started{},layout_complete{},spawn_started{};
  const auto progress=[&](const ScenePreparationProgress& p) {
    if(p.stage==ScenePreparationStage::Layout) {
      require(p.completed<=p.requested,"world layout progress exceeded its actual primitive count");
      layout_started=true;if(p.requested && p.completed==p.requested)layout_complete=true;
    }
    if(p.stage==ScenePreparationStage::Spawn)spawn_started=true;
  };
  require(prepare_scene_world(request,result,error,&cancel,progress),error);
  require(layout_started && layout_complete && spawn_started,"world omitted truthful ordering/spawn stages");
  require(result.hierarchy_ready && result.scene.neighborhood_ready && result.scene.full_scene_ready && result.scene.cooked_parts==0 &&
      result.catalog!=request.catalog && result.raster_bytes>0 && result.representation_bytes>result.raster_bytes+result.ray_expansion_bytes &&
      result.representation_bytes<request.gpu_budget_bytes,"large world did not use complete shared representations");
  require(validate_scene_world_hierarchy(result.catalog,result.hierarchy,error),error);
  SceneCatalog prepared;require(read_scene_catalog(result.catalog,prepared,error),error);
  require(prepared.parts.size()==257 && prepared.instances.size()==2 && prepared.instances[1].transform[0]==-1 &&
      !prepared.primitives.front().triangle_order.empty() &&
      std::all_of(prepared.parts.begin(),prepared.parts.end(),[](const auto& p){return p.bounds_prepared && p.geometry.empty();}),
      "world preparation changed node coverage or eagerly cooked all fine geometry");
  require(prepared.parts.back().bounds==std::array<float,6>{4096,0,0,4100,0,4} && result.scene.collision_pairs==1 &&
      std::abs(result.spawn[1]-1.615f)<.05f,"world collision bounds or grounded spawn are incorrect");
  require(octaryn::content::resource_file_digest(request.catalog,error)==canonical &&
      octaryn::content::resource_file_digest(root/"floor.bin",error)==source_hash,"world preparation changed canonical/source files");
  require(read_scene_hierarchy(result.hierarchy,hierarchy,error) && hierarchy.complete && hierarchy.target_triangles==512,error);
  std::filesystem::path cache;require(scene_hierarchy_path(result.hierarchy,hierarchy.primitives.front().roots.front().coarse.file,cache,error),error);
  auto missing=cache;missing+=".unavailable";std::filesystem::rename(cache,missing);
  const bool accepted_missing=prepare_scene_world(request,result,error,&cancel);std::filesystem::rename(missing,cache);
  require(!accepted_missing && !result.hierarchy_ready,"large world sealed a missing root cache");
  request.collision_budget_bytes=1;
  require(!prepare_scene_world(request,result,error,&cancel) && result.scene.collision_bytes>1 && !result.hierarchy_ready,
      "large world accepted incomplete protected collision admission");
  request.collision_budget_bytes=512ull<<20;request.gpu_budget_bytes=1;
  require(!prepare_scene_world(request,result,error,&cancel) && result.representation_bytes>1 && !result.hierarchy_ready,
      "large world accepted an over-budget complete representation");
  std::puts("scene_world_preparation_tests passed=1 leaves=257 original_nodes=2 full_hierarchy=1 grounded_spawn=1 exact_collision=1 cancel_resume=1 canonical_unchanged=1 missing_root_rejected=1 memory_denial=1 fine_cook=0 device_as_admission=required");
  return 0;
}
