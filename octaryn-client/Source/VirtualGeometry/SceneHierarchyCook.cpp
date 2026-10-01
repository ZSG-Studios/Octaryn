#include "SceneHierarchyInternal.h"
#include "GeometryCoarse.h"
#include "GeometryCache.h"
#include "SceneOrder.h"
#include "FilePath.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
std::array<float,6> model_bounds(const MapModel& model) {
  std::array<float,6> result{FLT_MAX,FLT_MAX,FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};
  for(const auto& v:model.vertices)for(unsigned axis=0;axis<3;++axis) {
    result[axis]=std::min(result[axis],v.position[axis]);result[axis+3]=std::max(result[axis+3],v.position[axis]);
  }
  return result;
}
bool reuse(const std::filesystem::path& file,const std::string& hash,GeometryAsset& asset,std::string& error) {
  if(!read_geometry_cache(file,hash,asset,error,true) || asset.space!=GeometrySpace::Object || asset.material_count!=1)return false;
  return std::all_of(asset.clusters.begin(),asset.clusters.end(),[](const auto& c){return c.refined_group==invalid_id;}) &&
      asset.groups.size()==1 && asset.roots.size()==1 && asset.roots.front()==0;
}
}
SceneHierarchyNode SceneHierarchyWork::leaf(std::uint32_t primitive,std::uint32_t part_id) {
  source.check();const auto& part=source.catalog.parts.at(part_id);const auto& p=source.catalog.primitives.at(primitive);
  require(part.primitive==primitive,"hierarchy leaf belongs to another material primitive");
  SceneHierarchyNode node;node.leaf_part=part_id;node.leaf_count=1;node.first_triangle=part.first_triangle;
  node.source_triangles=part.triangle_count;
  node.coverage_hash=hierarchy_digest(hierarchy.identity+":"+std::to_string(primitive)+":"+std::to_string(part_id)+":"+
      std::to_string(part.first_triangle)+":"+std::to_string(part.triangle_count));
  const auto hash=hierarchy_digest("hierarchy-leaf-coarse-v1:"+node.coverage_hash);
  const auto relative="coarse/"+hash+".vgeom";std::filesystem::path file;
  GeometryAsset asset;std::string error;
  if(!scene_hierarchy_path(request.output,relative,file,error))throw std::runtime_error(error);
  // Source bounds are read exactly even when reusing a valid coarse cache.
  if(!source.reader) {
    source.reader=std::make_unique<MapSourceReader>();
    if(!source.reader->open(preparation_path(source.catalog.source),request.output.parent_path()/"scratch",error,source.cancel))throw std::runtime_error(error);
  }
  MapModel model;
  if(!load_scene_part(*source.reader,source.path,source.catalog,part,model,error))throw std::runtime_error(error);
  check_snapshot(primitive);
  require(model.indices.size()/3==part.triangle_count && model.primitives.size()==1,"hierarchy source leaf is incomplete");
  node.bounds=model_bounds(model);
  if(!reuse(file,hash,asset,error)) {
    GeometryCoarseOptions options;options.position_only=p.position_only;options.target_triangles=request.target_triangles;
    options.maximum_triangles=request.maximum_triangles;options.cancel=source.cancel;
    if(!cook_coarse_geometry(model,hash,asset,error,options))throw std::runtime_error(error);
    check_snapshot(primitive);if(!write_geometry_cache(file,asset,error))throw std::runtime_error(error);
  }
  node.coarse=describe_hierarchy_geometry(relative,asset);source.check();return node;
}
SceneHierarchyNode SceneHierarchyWork::parent(const SceneHierarchyPrimitive& primitive,const SceneHierarchyShard& shard,
    std::span<const std::uint32_t> children) {
  source.check();SceneHierarchyNode node;node.children.assign(children.begin(),children.end());
  node.bounds={FLT_MAX,FLT_MAX,FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};
  std::string coverage=hierarchy.identity+":parent:",geometry;float inherited{};
  for(const auto id:children) {
    const auto& child=shard.nodes.at(id);node.source_triangles+=child.source_triangles;node.leaf_count+=child.leaf_count;
    coverage+=child.coverage_hash;geometry+=child.coarse.hash;inherited=std::max(inherited,child.coarse.error);
    for(unsigned a=0;a<3;++a) {node.bounds[a]=std::min(node.bounds[a],child.bounds[a]);node.bounds[a+3]=std::max(node.bounds[a+3],child.bounds[a+3]);}
  }
  node.coverage_hash=hierarchy_digest(coverage);const auto hash=hierarchy_digest("hierarchy-parent-coarse-v1:"+node.coverage_hash+geometry);
  const auto relative="coarse/"+hash+".vgeom";std::filesystem::path file;
  GeometryAsset asset;std::string error;
  if(!scene_hierarchy_path(request.output,relative,file,error))throw std::runtime_error(error);
  if(!reuse(file,hash,asset,error)) {
    MapModel model;model.primitives.emplace_back();model.primitives.front().material=primitive.surface;
    for(const auto id:children) {
      const auto& child=shard.nodes.at(id);GeometryAsset input;std::filesystem::path path;
      if(!scene_hierarchy_path(request.output,child.coarse.file,path,error) ||
          !read_geometry_cache(path,child.coarse.hash,input,error) ||
          !append_geometry_roots(path,input,model,request.maximum_triangles,error,source.cancel))throw std::runtime_error(error);
    }
    GeometryCoarseOptions options;options.position_only=primitive.position_only;options.target_triangles=request.target_triangles;
    options.maximum_triangles=request.maximum_triangles;options.inherited_error=inherited;options.cancel=source.cancel;
    if(!cook_coarse_geometry(model,hash,asset,error,options))throw std::runtime_error(error);
    check_snapshot(primitive.index);if(!write_geometry_cache(file,asset,error))throw std::runtime_error(error);
  }
  node.coarse=describe_hierarchy_geometry(relative,asset);source.check();return node;
}
void SceneHierarchyWork::prepare_primitive(std::uint32_t id) {
  source.check();auto& primitive=hierarchy.primitives.at(id);std::string error;SceneHierarchyShard shard;
  if(primitive.complete) {
    if(!read_scene_hierarchy_shard(request.output,hierarchy,id,shard,error))throw std::runtime_error(error);
    return;
  }
  if(!read_hierarchy_work(request.output,hierarchy,id,shard,error)) {
    shard={};shard.primitive=id;shard.identity=hierarchy.identity;shard.source_hash=hierarchy.source_hash;shard.order_hash=primitive.order_hash;
  }
  const auto& original=source.catalog.primitives[id];std::uint32_t leaves{};
  for(const auto& n:shard.nodes)leaves+=n.children.empty()?1:0;
  progress.completed_leaves+=leaves;
  for(;leaves<original.part_count;++leaves) {
    if(new_leaves>=request.maximum_new_leaves)return;
    auto node=leaf(id,original.first_part+leaves);node.id=unsigned(shard.nodes.size());
    std::printf("scene_hierarchy_leaf primitive=%u part=%u source_triangles=%llu coarse_triangles=%llu pages=%u error=%.9g\n",
        id,node.leaf_part,static_cast<unsigned long long>(node.source_triangles),static_cast<unsigned long long>(node.coarse.triangles),node.coarse.pages,double(node.coarse.error));
    std::fflush(stdout);
    shard.roots.push_back(node.id);shard.nodes.push_back(std::move(node));
    check_snapshot(id);
    if(!write_hierarchy_work(request.output,hierarchy,shard,error))throw std::runtime_error(error);
    progress.completed_leaves++;new_leaves++;report();
  }
  // Roots form a disjoint exact cover throughout every checkpoint and replacement.
  while(shard.roots.size()>1) {
    source.check();const auto before=shard.roots.size();
    // Verified Morton ranges already form spatial batches; retain that ordering
    // through each level so one-axis sorting cannot scatter adjoining patches.
    if(primitive.order_hash.empty()) {
      unsigned axis{};for(unsigned a=1;a<3;++a)if(primitive.bounds[a+3]-primitive.bounds[a]>primitive.bounds[axis+3]-primitive.bounds[axis])axis=a;
      std::stable_sort(shard.roots.begin(),shard.roots.end(),[&](auto a,auto b){
        const auto& x=shard.nodes[a].bounds;const auto& y=shard.nodes[b].bounds;return double(x[axis])+x[axis+3]<double(y[axis])+y[axis+3];});
    }
    std::vector<unsigned> next;
    for(std::size_t first=0;first<shard.roots.size();) {
      std::size_t count=1;std::uint64_t triangles=shard.nodes[shard.roots[first]].coarse.triangles;
      while(first+count<shard.roots.size() && count<request.fan_in &&
          triangles+shard.nodes[shard.roots[first+count]].coarse.triangles<=request.maximum_triangles)
        triangles+=shard.nodes[shard.roots[first+count++]].coarse.triangles;
      if(count==1)next.push_back(shard.roots[first]);
      else {
        auto node=parent(primitive,shard,std::span(shard.roots).subspan(first,count));node.id=unsigned(shard.nodes.size());
        next.push_back(node.id);shard.nodes.push_back(std::move(node));
      }
      first+=count;
    }
    shard.roots=std::move(next);
    check_snapshot(id);
    if(!write_hierarchy_work(request.output,hierarchy,shard,error))throw std::runtime_error(error);
    if(shard.roots.size()==before)break; // Unmergeable complete forest remains explicit.
  }
  shard.complete=true;
  if(!write_hierarchy_work(request.output,hierarchy,shard,error) || !write_scene_hierarchy_shard(request.output,hierarchy,shard,error))throw std::runtime_error(error);
  // The immutable shard key is its exact serialized digest, checked by the lazy reader.
  primitive.roots.clear();for(auto root:shard.roots)primitive.roots.push_back(shard.nodes[root]);
  primitive.complete=true;
  checkpoint();report();
}
}
