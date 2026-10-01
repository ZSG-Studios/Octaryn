#include "ScenePreparationHierarchyInternal.h"
#include "SceneHierarchyInternal.h"
#include "SceneRootPages.h"
#include "SceneSelectionBudget.h"
#include "SceneRasterBudget.h"
#include "GeometryCache.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
void check(const std::atomic_bool* cancel) {
  require(!cancel || !cancel->load(std::memory_order_relaxed),"scene world preparation canceled");
}
void add(std::uint64_t& total,std::uint64_t bytes) {
  require(bytes<=UINT64_MAX-total,"scene world memory accounting overflow");total+=bytes;
}
SelectionResourcesConfig configuration(const SceneCatalog& catalog) {
  SelectionResourcesConfig result;result.groups=4096;result.clusters=16384;result.pages=2048;
  result.page_references=32768;result.parents=65536;result.feedback_capacity=128;result.instances=1;
  std::vector<unsigned> counts(catalog.mesh_count);
  for(const auto& node:catalog.instances)result.instances=std::max(result.instances,++counts.at(node.mesh));
  require(result.instances<=65536,"original node set exceeds shared instance selection capacity");
  return result;
}
void root_geometry(const std::filesystem::path& package,const SceneHierarchyNode& node,
    const ScenePrimitive& primitive,const SelectionResourcesConfig& config,std::uint64_t nodes,
    ScenePreparationWorldResult& result,std::vector<unsigned>& payloads,std::uint64_t& peak_copy_bytes,
    std::uint64_t& feedback_bytes,std::uint64_t& pages,SceneRasterCapacity& raster) {
  std::string error;std::filesystem::path path;
  require(scene_hierarchy_path(package,node.coarse.file,path,error),error);
  GeometryAsset asset;require(read_geometry_cache(path,node.coarse.hash,asset,error,true),error);
  const auto actual=describe_hierarchy_geometry(node.coarse.file,asset);
  const auto& expected=node.coarse;
  require(asset.space==GeometrySpace::Object && asset.material_count==1 && actual.triangles==expected.triangles &&
      actual.pages==expected.pages && actual.clusters==expected.clusters && actual.metadata_bytes==expected.metadata_bytes &&
      actual.encoded_bytes==expected.encoded_bytes && actual.root_page_ids==expected.root_page_ids &&
      actual.page_used_bytes==expected.page_used_bytes && actual.error==expected.error,"prepared hierarchy root cache differs from its sealed summary");
  SelectionTopology topology;require(build_selection_topology(asset,topology,error),error);
  require(topology.groups.size()<=config.groups && topology.clusters.size()<=config.clusters && asset.pages.size()<=config.pages &&
      topology.pages.size()<=config.page_references && topology.parents.size()<=config.parents,
      "complete hierarchy root exceeds shared selection capacity");
  std::uint64_t vertices{},triangles{};bool compact=true;
  std::vector<bool> root_clusters(asset.clusters.size());
  for(auto root:asset.roots) {
    const auto& group=asset.groups.at(root);
    for(unsigned i=0;i<group.cluster_count;++i)root_clusters.at(group.first_cluster+i)=true;
  }
  for(unsigned i=0;i<asset.clusters.size();++i)if(root_clusters[i]) {
    const auto& cluster=asset.clusters[i];vertices+=cluster.vertex_count;triangles+=cluster.triangle_count;
    compact=compact && (cluster.flags&geometry_position_only)!=0;
    require(cluster.material==0 && (cluster.flags&3u)==unsigned(primitive.surface.alpha_mode),
        "prepared hierarchy root changes authored material coverage");
  }
  const auto clusters=std::count(root_clusters.begin(),root_clusters.end(),true);
  require(triangles==actual.triangles,"prepared root cut omits coarse representation triangles");
  add(pages,asset.pages.size());
  add(raster.clusters,asset.clusters.size());add(raster.pages,asset.pages.size());
  add(raster.instances,nodes);add(raster.draws,asset.clusters.size()*nodes);
  const bool complete_root_cut=std::size_t(clusters)==asset.clusters.size() &&
      std::all_of(asset.clusters.begin(),asset.clusters.end(),[](const auto& c){return c.refined_group==invalid_id;});
  if(!complete_root_cut)
    add(feedback_bytes,scene_selection_feedback_bytes(unsigned(asset.pages.size()),std::min(unsigned(asset.pages.size()),config.feedback_capacity)));
  const std::uint64_t batches=(std::uint64_t(clusters)+127)/128;
  const auto expansion=vertices*(compact?12:72)+triangles*28+batches*16;
  add(result.ray_expansion_bytes,expansion);
  add(result.representation_bytes,std::uint64_t(asset.clusters.size())*sizeof(GeometryCluster));
  // Current + previous world descriptors and two native instance upload frames.
  add(result.representation_bytes,nodes*batches*(2*160+2*64));
  if(primitive.surface.alpha_mode==MapAlphaMode::Blend)add(result.representation_bytes,triangles*3*(80+8));
  peak_copy_bytes=std::max(peak_copy_bytes,std::uint64_t(clusters)*48+asset.pages.size()*16+4);
  for(auto page:actual.root_page_ids) {payloads.push_back(actual.page_used_bytes.at(page));add(result.root_payload_bytes,payloads.back());}
  add(result.root_triangles,triangles);
}
}
void prepare_world_hierarchy(const ScenePreparationRequest& request,SceneCatalog& catalog,ScenePreparationWorldResult& result,
    const std::atomic_bool* cancel,const ScenePreparationNotify& notify,std::uint32_t target_triangles) {
  result.hierarchy=request.catalog.parent_path()/"hierarchy"/"scene.json";
  SceneHierarchyRequest options;options.catalog=request.catalog;options.output=result.hierarchy;options.target_triangles=target_triangles;
  SceneHierarchyProgress progress;std::string error;
  require(prepare_scene_hierarchy(options,progress,error,cancel,[&](const auto& p) {
    check(cancel);if(notify)notify({ScenePreparationStage::Geometry,p.completed_leaves,p.total_leaves,p.total_leaves,p.completed_leaves,0});
  }),error);
  check(cancel);SceneHierarchy hierarchy;require(read_scene_hierarchy(result.hierarchy,hierarchy,error),error);
  require(progress.complete && hierarchy.complete && hierarchy.identity==hierarchy_layout_identity(catalog,options),
      "prepared hierarchy does not cover the complete original world");
  const auto config=configuration(catalog);std::vector<unsigned> payloads;std::vector<bool> covered(catalog.parts.size());
  std::vector<std::uint64_t> nodes(catalog.mesh_count);for(const auto& node:catalog.instances)++nodes.at(node.mesh);
  result.representation_bytes=scene_selection_bytes(config)+std::uint64_t(catalog.primitives.size())*304*16;
  std::uint64_t peak_copy_bytes{},roots{},feedback_bytes{},pages{};SceneRasterCapacity raster;
  for(unsigned p=0;p<catalog.primitives.size();++p) {
    check(cancel);SceneHierarchyShard shard;require(read_scene_hierarchy_shard(result.hierarchy,hierarchy,p,shard,error),error);
    require(shard.complete,"prepared hierarchy shard is incomplete");
    const auto& primitive=catalog.primitives[p];
    for(const auto& node:shard.nodes)if(node.children.empty()) {
      require(node.leaf_part>=primitive.first_part && node.leaf_part<primitive.first_part+primitive.part_count &&
          node.leaf_part<catalog.parts.size() && !covered[node.leaf_part],"hierarchy collision leaf identity differs");
      auto& part=catalog.parts[node.leaf_part];
      require(part.primitive==p && part.first_triangle==node.first_triangle && part.triangle_count==node.source_triangles,
          "hierarchy collision leaf omits original source triangles");
      part.bounds=node.bounds;part.bounds_prepared=true;covered[node.leaf_part]=true;
    }
    for(const auto& root:hierarchy.primitives[p].roots) {
      check(cancel);root_geometry(result.hierarchy,root,primitive,config,nodes.at(primitive.mesh),result,payloads,peak_copy_bytes,feedback_bytes,pages,raster);++roots;
    }
    if(notify)notify({ScenePreparationStage::Bounds,p+1,catalog.primitives.size(),catalog.parts.size(),
        std::uint64_t(std::count(covered.begin(),covered.end(),true)),0});
  }
  require(std::all_of(covered.begin(),covered.end(),[](bool v){return v;}),"prepared collision catalog omits source leaves");
  const auto slots=SceneRootPages::required_slots(payloads);
  require(slots!=invalid_id && slots<=8192-256 && roots<=65536-512,"complete hierarchy exceeds shared root capacity");
  require(pages<=262144 && feedback_bytes<=config.readback_bytes,"complete hierarchy exceeds shared page or selection feedback capacity");
  result.raster_bytes=scene_raster_bytes(raster);
  require(result.raster_bytes!=UINT64_MAX,"complete hierarchy exceeds shared raster identity capacity");
  add(result.representation_bytes,result.raster_bytes);
  add(result.representation_bytes,std::uint64_t(std::max(slots,1u)+256)*page_bytes);
  add(result.representation_bytes,result.ray_expansion_bytes);add(result.representation_bytes,peak_copy_bytes);
  result.scene.render_bytes=result.representation_bytes;result.scene.render_parts=roots;
  result.scene.prepared_bounds=catalog.parts.size();result.scene.cooked_parts=0;
  require(result.representation_bytes<request.gpu_budget_bytes,"complete hierarchy representation floor exceeds aggregate render budget");
  std::printf("scene_world_prepared_roots roots=%llu triangles=%llu packed_slots=%u representation_bytes=%llu ray_expansion_bytes=%llu raster_bytes=%llu budget=%llu device_as_admission=required\n",
      static_cast<unsigned long long>(roots),static_cast<unsigned long long>(result.root_triangles),slots,
      static_cast<unsigned long long>(result.representation_bytes),static_cast<unsigned long long>(result.ray_expansion_bytes),
      static_cast<unsigned long long>(result.raster_bytes),
      static_cast<unsigned long long>(request.gpu_budget_bytes));
}
}
