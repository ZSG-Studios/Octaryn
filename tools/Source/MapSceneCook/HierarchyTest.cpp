#include "SceneHierarchy.h"
#include "SceneHierarchyDetail.h"
#include "SceneCatalog.h"
#include "SceneOrder.h"
#include "GeometryCoarse.h"
#include "GeometryCache.h"
#include "GeometryMesh.h"
#include "ResourceDigest.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>

void test_coarse_faces(const std::filesystem::path&);
namespace {
void require(bool valid,const std::string& error) {if(!valid)throw std::runtime_error(error);}
void test_position_weld() {
  using namespace octaryn::client::rendering;
  using namespace octaryn::client::rendering::virtual_geometry;
  constexpr float positions[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
  constexpr unsigned indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
  MapModel model;model.primitives.emplace_back();model.primitives[0].index_count=36;
  for(unsigned i=0;i<36;++i) {
    MapVertex vertex{};std::copy_n(positions[indices[i]],3,vertex.position);
    const auto face=i/6;vertex.normal[face%3]=face<3?1.f:-1.f;
    model.indices.push_back(i);model.vertices.push_back(vertex);
  }
  const auto compact=geometry_mesh(model,model.primitives[0],true);
  const auto authored=geometry_mesh(model,model.primitives[0],false);
  require(compact.vertices.size()==8 && std::none_of(compact.locks.begin(),compact.locks.end(),[](auto v){return v!=0;}),
      "generated normals kept artificial boundaries in a position-only closed mesh");
  require(authored.vertices.size()==24 && std::all_of(authored.locks.begin(),authored.locks.end(),[](auto v){return v!=0;}),
      "authored normal seams were discarded during exact weld");
}
void source_fixture(const std::filesystem::path& root) {
  constexpr unsigned size=32;
  std::vector<float> positions,normals,uvs;std::vector<unsigned> indices;
  for(unsigned z=0;z<=size;++z)for(unsigned x=0;x<=size;++x) {
    positions.insert(positions.end(),{float(x),0,float(z)});normals.insert(normals.end(),{0,1,0});uvs.insert(uvs.end(),{float(x)/size,float(z)/size});
  }
  for(unsigned z=0;z<size;++z)for(unsigned x=0;x<size;++x) {
    const auto a=z*(size+1)+x,b=a+1,c=a+size+1,d=c+1;
    indices.insert(indices.end(),{a,c,b,b,c,d});
  }
  const auto position_bytes=positions.size()*4,index_bytes=indices.size()*4,normal_bytes=normals.size()*4,uv_bytes=uvs.size()*4;
  std::ofstream data(root/"grid.bin",std::ios::binary);
  data.write(reinterpret_cast<const char*>(positions.data()),position_bytes);data.write(reinterpret_cast<const char*>(indices.data()),index_bytes);
  data.write(reinterpret_cast<const char*>(normals.data()),normal_bytes);data.write(reinterpret_cast<const char*>(uvs.data()),uv_bytes);data.close();
  std::ofstream gltf(root/"grid.gltf");
  gltf<<"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"grid.bin\",\"byteLength\":"<<position_bytes+index_bytes+normal_bytes+uv_bytes<<"}],\"bufferViews\":["
      <<"{\"buffer\":0,\"byteLength\":"<<position_bytes<<"},"
      <<"{\"buffer\":0,\"byteOffset\":"<<position_bytes<<",\"byteLength\":"<<index_bytes<<"},"
      <<"{\"buffer\":0,\"byteOffset\":"<<position_bytes+index_bytes<<",\"byteLength\":"<<normal_bytes<<"},"
      <<"{\"buffer\":0,\"byteOffset\":"<<position_bytes+index_bytes+normal_bytes<<",\"byteLength\":"<<uv_bytes<<"}],"
      <<"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":1089,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[32,0,32]},"
      <<"{\"bufferView\":1,\"componentType\":5125,\"count\":6144,\"type\":\"SCALAR\"},"
      <<"{\"bufferView\":2,\"componentType\":5126,\"count\":1089,\"type\":\"VEC3\"},"
      <<"{\"bufferView\":3,\"componentType\":5126,\"count\":1089,\"type\":\"VEC2\"}],"
      <<R"("materials":[{"alphaMode":"OPAQUE"},{"alphaMode":"MASK","alphaCutoff":0.5},{"alphaMode":"BLEND"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0},
{"attributes":{"POSITION":0,"NORMAL":2,"TEXCOORD_0":3},"indices":1,"material":1},
{"attributes":{"POSITION":0,"NORMAL":2,"TEXCOORD_0":3},"indices":1,"material":2}]}],
"nodes":[{"mesh":0},{"mesh":0,"matrix":[-1,0,0,0,0.2,2,0,0,0,0,1,0,64,0,0,1]}],"scenes":[{"nodes":[0,1]}],"scene":0})";
}
}
int test_scene_hierarchy(const std::filesystem::path& root) {
  using namespace octaryn::client::rendering;
  using namespace octaryn::client::rendering::virtual_geometry;
  test_position_weld();
  std::filesystem::create_directories(root);source_fixture(root);
  test_coarse_faces(root);
  std::string error;SceneCatalog catalog;
  require(import_scene_catalog(root/"grid.gltf",catalog,error),error);
  catalog.part_triangles=128;catalog.parts.clear();
  for(unsigned p=0;p<catalog.primitives.size();++p) {
    auto& primitive=catalog.primitives[p];primitive.first_part=unsigned(catalog.parts.size());primitive.part_count=16;
    for(unsigned i=0;i<16;++i) {ScenePart part;part.primitive=p;part.first_triangle=i*128;part.triangle_count=128;part.bounds=primitive.bounds;catalog.parts.push_back(part);}
  }
  const auto canonical=root/"catalog.json";require(write_scene_catalog(canonical,catalog,error),error);
  const auto source_hash=octaryn::content::resource_file_digest(root/"grid.bin",error);
  const auto catalog_hash=octaryn::content::resource_file_digest(canonical,error);
  SceneHierarchyRequest request;request.catalog=canonical;request.output=root/"hierarchy"/"scene.json";request.target_triangles=32;
  SceneHierarchyProgress progress;std::atomic_bool cancel{};
  require(!prepare_scene_hierarchy(request,progress,error,&cancel,[&](const auto& p){if(p.completed_leaves==2)cancel=true;}) && progress.canceled,
      "hierarchy cancellation did not stop bounded preparation");
  SceneHierarchy header;require(read_scene_hierarchy(request.output,header,error) && !header.complete && header.primitives[0].roots.empty(),
      "hierarchy published partial source coverage");
  cancel=false;require(prepare_scene_hierarchy(request,progress,error,&cancel),error);
  require(read_scene_hierarchy(request.output,header,error) && header.complete && header.instances.size()==2 && progress.completed_leaves==48,error);
  require(header.unique_triangles==6144 && header.instanced_triangles==12288 && progress.forest_roots==3 && progress.root_pages<48,
      "hierarchy lost source instances or retained every leaf root");
  std::uint64_t covered{};std::vector<std::filesystem::file_time_type> modified;
  for(unsigned p=0;p<header.primitives.size();++p) {
    SceneHierarchyShard shard;require(read_scene_hierarchy_shard(request.output,header,p,shard,error),error);
    require(shard.complete && shard.roots.size()==1 && shard.nodes.size()>16,"hierarchy did not cross part boundaries");
    auto broken=shard;broken.nodes.back().children.push_back(broken.nodes.back().children.front());
    require(!validate_scene_hierarchy_shard(header,broken,error),"hierarchy accepted duplicate replacement coverage");
    const auto& node=shard.nodes.at(shard.roots.front());covered+=node.source_triangles;
    std::filesystem::path file;require(scene_hierarchy_path(request.output,node.coarse.file,file,error),error);
    GeometryAsset asset;require(read_geometry_cache(file,node.coarse.hash,asset,error,true),error);modified.push_back(std::filesystem::last_write_time(file));
    require(asset.space==GeometrySpace::Object && asset.source_triangles==node.coarse.triangles && node.coarse.triangles<node.source_triangles,
        "hierarchy relabeled original triangles or failed to simplify");
    MapModel coarse;coarse.primitives.emplace_back();coarse.primitives.front().material=header.primitives[p].surface;
    require(append_geometry_roots(file,asset,coarse,262144,error),error);
    std::set<std::pair<float,float>> boundary;
    for(const auto& v:coarse.vertices) {
      if(v.position[0]==0 || v.position[0]==32 || v.position[2]==0 || v.position[2]==32)boundary.emplace(v.position[0],v.position[2]);
      if(p)require(v.normal[1]==1 && v.uv[0]==v.position[0]/32 && v.uv[1]==v.position[2]/32,"coarse hierarchy changed authored attributes");
    }
    require(boundary.size()==128,"hierarchy moved or removed exterior seam vertices");
    for(const auto& c:asset.clusters)require((c.flags&3u)==p && bool(c.flags&geometry_position_only)==(p==0),"hierarchy material/normal mode changed");
  }
  require(covered==6144,"hierarchy roots do not cover all source domains");
  require(progress.root_payload_bytes<progress.root_pages*page_bytes && progress.root_payload_bytes>0,
      "hierarchy root payload summary assumes a whole page per root");
  SceneHierarchyDetail detail;require(detail.open(request.output,error,&cancel),error);
  for(unsigned p=0;p<header.primitives.size();++p) {
    SceneHierarchyShard shard;require(read_scene_hierarchy_shard(request.output,header,p,shard,error),error);
    SceneHierarchyGeometry exact;require(detail.prepare(p,0,exact,error),error);
    std::filesystem::path file;require(scene_hierarchy_path(request.output,exact.file,file,error),error);
    GeometryAsset asset;require(read_geometry_cache(file,exact.hash,asset,error,true),error);
    require(asset.source_triangles==128 && exact.triangles==128 && asset.material_count==1 && asset.space==GeometrySpace::Object,
        "lazy exact leaf did not restore the original triangle domain");
    const auto time=std::filesystem::last_write_time(file);require(detail.prepare(p,0,exact,error),error);
    require(std::filesystem::last_write_time(file)==time,"exact leaf resume rewrote existing geometry");
  }
  cancel=true;SceneHierarchyGeometry canceled_detail;
  require(!detail.prepare(0,1,canceled_detail,error) && canceled_detail.file.empty(),"canceled exact leaf published geometry");cancel=false;
  const auto shards=request.output.parent_path()/"shards",hidden=request.output.parent_path()/"shards-hidden";
  std::filesystem::rename(shards,hidden);
  require(read_scene_hierarchy(request.output,header,error),"root summary eagerly read descendant shards");
  std::filesystem::rename(hidden,shards);
  require(prepare_scene_hierarchy(request,progress,error),error);
  for(unsigned p=0;p<header.primitives.size();++p) {
    std::filesystem::path file;require(scene_hierarchy_path(request.output,header.primitives[p].roots[0].coarse.file,file,error),error);
    require(std::filesystem::last_write_time(file)==modified[p],"hierarchy resume rewrote valid coarse caches");
  }
  require(octaryn::content::resource_file_digest(root/"grid.bin",error)==source_hash &&
      octaryn::content::resource_file_digest(canonical,error)==catalog_hash,"hierarchy changed source or canonical catalog");
  const auto ordered=root/"ordered"/"catalog.json";
  require(write_scene_catalog(ordered,catalog,error) && prepare_scene_spatial_order(ordered,0,3,error),error);
  SceneCatalog ordered_catalog;require(read_scene_catalog(ordered,ordered_catalog,error),error);
  const auto order_file=ordered.parent_path()/ordered_catalog.primitives[0].triangle_order;
  const auto order_stamp=std::filesystem::last_write_time(order_file);
  SceneHierarchyRequest modified_order=request;modified_order.catalog=ordered;modified_order.output=root/"order-mutation"/"scene.json";
  SceneHierarchyProgress mutation_progress;
  const auto changed=prepare_scene_hierarchy(modified_order,mutation_progress,error,nullptr,[&](const auto& p){
    if(p.completed_leaves==1)std::filesystem::last_write_time(order_file,order_stamp+std::chrono::seconds(1));
  });
  std::filesystem::last_write_time(order_file,order_stamp);
  require(!changed && error.find("order changed")!=std::string::npos,"hierarchy published after source order mutation");
  SceneHierarchy rejected;require(read_scene_hierarchy(modified_order.output,rejected,error) && rejected.primitives[0].roots.empty(),
      "changed source order exposed incomplete primitive roots");
  require(std::distance(std::filesystem::directory_iterator(modified_order.output.parent_path()/"coarse"),std::filesystem::directory_iterator{})==1,
      "source order mutation published a stale-key geometry cache");
  SceneHierarchyRequest spatial=request;spatial.catalog=ordered;spatial.output=root/"ordered-hierarchy"/"scene.json";
  SceneHierarchyProgress spatial_progress;require(prepare_scene_hierarchy(spatial,spatial_progress,error),error);
  SceneHierarchy spatial_header;require(read_scene_hierarchy(spatial.output,spatial_header,error),error);
  for(unsigned p=0;p<spatial_header.primitives.size();++p) {
    SceneHierarchyShard shard;require(read_scene_hierarchy_shard(spatial.output,spatial_header,p,shard,error),error);
    std::vector<std::pair<std::uint64_t,std::uint64_t>> ranges;
    for(const auto& node:shard.nodes) {
      auto first=node.first_triangle,end=first+node.source_triangles;
      if(!node.children.empty()) {
        first=ranges.at(node.children.front()).first;end=first;
        for(const auto child:node.children) {
          require(ranges.at(child).first==end,"hierarchy scattered verified contiguous Morton domains");
          end=ranges.at(child).second;
        }
      }
      require(end-first==node.source_triangles,"hierarchy Morton parent omitted its domain");ranges.emplace_back(first,end);
    }
  }
  require(spatial_header.identity!=header.identity && spatial_progress.complete,"Morton identity or complete coverage was lost");
  require(octaryn::content::resource_file_digest(canonical,error)==catalog_hash,"Morton preparation changed original canonical catalog");
  std::printf("scene_hierarchy_tests passed=1 source_triangles=6144 instanced_triangles=12288 leaves=48 roots=%llu root_pages=%llu root_triangles=%llu root_payload_bytes=%llu cancellation=1 resume=1 lazy_metadata=1 lazy_exact_detail=1 exterior_locks=1 materials=3 original_nodes=2\n",
      static_cast<unsigned long long>(progress.forest_roots),static_cast<unsigned long long>(progress.root_pages),static_cast<unsigned long long>(progress.root_triangles),
      static_cast<unsigned long long>(progress.root_payload_bytes));
  std::printf("scene_hierarchy_morton_contiguous passed=1 complete_source=1 primitives=3 original_nodes=2 canonical_unchanged=1\n");
  return 0;
}
