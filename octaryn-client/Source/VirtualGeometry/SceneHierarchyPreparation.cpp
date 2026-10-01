#include "SceneHierarchyInternal.h"
#include "ScenePreparationLock.h"
#include "SceneOrder.h"
#include "FilePath.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
void SceneHierarchyWork::snapshot_orders() {
  order_stamps.clear();
  for(unsigned id=0;id<source.catalog.primitives.size();++id)if(const auto& primitive=source.catalog.primitives[id];!primitive.triangle_order.empty()) {
    source.check();std::filesystem::path path;std::string error;
    if(!scene_geometry::spatial_triangle_order_path(source.path,preparation_path(primitive.triangle_order),path,error))
      throw std::runtime_error(error);
    const auto file=content::file_io_path(path);
    order_stamps.push_back({id,path,std::filesystem::last_write_time(file),std::filesystem::file_size(file)});
  }
}
void SceneHierarchyWork::check_snapshot(std::uint32_t primitive) {
  source.check();
  if(source.resource_times.size()!=source.catalog.resources.size())throw std::runtime_error("hierarchy source snapshot incomplete");
  for(std::size_t i=0;i<source.resource_times.size();++i) {
    const auto& resource=source.catalog.resources[i];const auto path=content::file_io_path(preparation_path(resource.path));
    if(std::filesystem::last_write_time(path)!=source.resource_times[i] || std::filesystem::file_size(path)!=resource.bytes)
      throw std::runtime_error("hierarchy source changed; checkpoint rejected");
  }
  for(const auto& order:order_stamps)if(primitive==invalid_id || order.primitive==primitive) {
    const auto file=content::file_io_path(order.path);
    if(std::filesystem::last_write_time(file)!=order.time || std::filesystem::file_size(file)!=order.bytes)
      throw std::runtime_error("hierarchy source order changed; checkpoint rejected");
  }
}
void SceneHierarchyWork::checkpoint() {
  check_snapshot();hierarchy.complete=std::all_of(hierarchy.primitives.begin(),hierarchy.primitives.end(),[](const auto& p){return p.complete;});
  std::string error;if(!write_scene_hierarchy(request.output,hierarchy,error))throw std::runtime_error(error);
}
void SceneHierarchyWork::report() {
  progress.completed_primitives=0;progress.total_primitives=hierarchy.primitives.size();progress.total_leaves=hierarchy.leaf_parts;
  progress.root_pages=progress.root_triangles=progress.root_metadata_bytes=progress.root_encoded_bytes=progress.root_payload_bytes=progress.forest_roots=0;
  for(const auto& p:hierarchy.primitives) {
    progress.completed_primitives+=p.complete?1:0;
    for(const auto& n:p.roots) {
      ++progress.forest_roots;progress.root_pages+=n.coarse.root_page_ids.size();progress.root_triangles+=n.coarse.triangles;
      progress.root_metadata_bytes+=n.coarse.metadata_bytes;progress.root_encoded_bytes+=n.coarse.encoded_bytes;
      for(auto page:n.coarse.root_page_ids)progress.root_payload_bytes+=n.coarse.page_used_bytes.at(page);
    }
  }
  progress.complete=hierarchy.complete;progress.canceled=source.cancel && source.cancel->load(std::memory_order_relaxed);
  if(notify)notify(progress);
}
bool prepare_scene_hierarchy(const SceneHierarchyRequest& request,SceneHierarchyProgress& result,std::string& error,
    const std::atomic_bool* cancel,SceneHierarchyNotify notify) {
  SceneHierarchyWork work;work.request=request;work.source.path=request.catalog;work.source.cancel=cancel;work.notify=std::move(notify);
  try {
    ScenePreparationLock lock(request.output);
    work.source.check();
    if(!read_scene_catalog(request.catalog,work.source.catalog,error))throw std::runtime_error(error);
    const auto& catalog=work.source.catalog;
    const auto identity=hierarchy_layout_identity(catalog,request);
    if(std::filesystem::exists(content::file_io_path(request.output))) {
      if(!read_scene_hierarchy(request.output,work.hierarchy,error))throw std::runtime_error(error);
      if(work.hierarchy.identity!=identity)throw std::runtime_error("hierarchy source/order/cooker options changed; choose a new cache package");
    } else {
      auto& h=work.hierarchy;h.source=catalog.source;h.source_hash=catalog.source_hash;h.identity=identity;
      const auto catalog_path=content::canonical_file_path(request.catalog).generic_u8string();
      h.catalog.assign(reinterpret_cast<const char*>(catalog_path.data()),catalog_path.size());
      h.target_triangles=request.target_triangles;h.fan_in=request.fan_in;h.maximum_triangles=request.maximum_triangles;
      h.unique_triangles=catalog.unique_triangles;h.instanced_triangles=catalog.instanced_triangles;h.leaf_parts=catalog.parts.size();
      h.resources=catalog.resources;h.instances=catalog.instances;
      for(unsigned i=0;i<catalog.primitives.size();++i) {
        const auto& original=catalog.primitives[i];SceneHierarchyPrimitive primitive;
        primitive.index=i;primitive.mesh=original.mesh;primitive.primitive=original.primitive;primitive.material=original.material;
        primitive.part_count=original.part_count;primitive.source_triangles=original.triangles;primitive.position_only=original.position_only;
        primitive.bounds=original.bounds;primitive.surface=original.surface;primitive.order_file=original.triangle_order;primitive.order_hash=original.triangle_order_hash;
        h.primitives.push_back(std::move(primitive));
      }
    }
    for(const auto& primitive:work.hierarchy.primitives)if(primitive.complete)work.progress.completed_leaves+=primitive.part_count;
    work.snapshot_orders();work.source.verify_resources();work.checkpoint();work.report();
    if(request.first_primitive>work.hierarchy.primitives.size())throw std::runtime_error("hierarchy primitive range exceeds source");
    const auto end=request.first_primitive+std::min<std::uint64_t>(request.primitive_count,work.hierarchy.primitives.size()-request.first_primitive);
    for(auto i=request.first_primitive;i<end && work.new_leaves<request.maximum_new_leaves;++i)work.prepare_primitive(std::uint32_t(i));
    work.checkpoint();work.report();result=work.progress;error.clear();return true;
  }catch(const std::exception& failure) {
    error=failure.what();try {work.report();}catch(...) {}result=work.progress;return false;
  }
}
}
