#include "ScenePreparationInternal.h"
#include "SceneResourceHash.h"
#include "SceneOrder.h"
#include "FilePath.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
void ScenePreparationWork::check() const {
  if(cancel && cancel->load(std::memory_order_relaxed))throw std::runtime_error("scene preparation canceled");
}
void ScenePreparationWork::verify_resources() {
  check();report(ScenePreparationStage::Inspect,0,catalog.resources.size());
  resource_times.clear();
  std::uint64_t completed{};
  for(const auto& resource:catalog.resources) {
    check();std::string error;const auto source=content::file_io_path(preparation_path(resource.path));
    const auto stamp=std::filesystem::last_write_time(source);
    if(std::filesystem::file_size(source)!=resource.bytes || scene_resource_hash(source,error,cancel)!=resource.hash)
      throw std::runtime_error(error.empty()?"scene source changed; regenerate its catalog before preparing":error);
    if(std::filesystem::last_write_time(source)!=stamp)throw std::runtime_error("scene source changed during preparation validation");
    resource_times.push_back(stamp);
    report(ScenePreparationStage::Inspect,++completed,catalog.resources.size());
  }
  std::string error;if(!verify_scene_orders(path,catalog,error,cancel))throw std::runtime_error(error);
}
void ScenePreparationWork::checkpoint(bool force) {
  if(!modified || (!force && std::chrono::steady_clock::now()-saved<std::chrono::seconds(2)))return;
  if(resource_times.size()!=catalog.resources.size())throw std::runtime_error("scene source snapshot is incomplete");
  for(std::size_t i=0;i<resource_times.size();++i) {
    const auto source=content::file_io_path(preparation_path(catalog.resources[i].path));
    if(std::filesystem::last_write_time(source)!=resource_times[i] || std::filesystem::file_size(source)!=catalog.resources[i].bytes)
      throw std::runtime_error("scene source changed during preparation; checkpoint rejected");
  }
  std::string error;if(!write_scene_catalog(path,catalog,error))throw std::runtime_error(error);
  saved=std::chrono::steady_clock::now();modified=false;
}
void ScenePreparationWork::finish() {
  result.total_parts=catalog.parts.size();result.prepared_bounds=result.cooked_parts=0;
  for(const auto& part:catalog.parts) {
    result.prepared_bounds+=part.bounds_prepared?1:0;result.cooked_parts+=part.geometry.empty()?0:1;
  }
  result.full_scene_ready=result.total_parts && result.prepared_bounds==result.total_parts && result.cooked_parts==result.total_parts;
  std::erase_if(result.pending_bounds,[&](auto id){return catalog.parts.at(id).bounds_prepared;});
  std::erase_if(result.pending_cooks,[&](auto id){return !catalog.parts.at(id).geometry.empty();});
}
void ScenePreparationWork::report(ScenePreparationStage stage,std::uint64_t completed,std::uint64_t requested) {
  finish();if(notify)notify({stage,completed,requested,result.total_parts,result.prepared_bounds,result.cooked_parts});
}
void ScenePreparationWork::bounds(std::span<const std::uint32_t> parts) {
  std::uint64_t completed{};report(ScenePreparationStage::Bounds,0,parts.size());
  for(const auto id:parts) {
    check();auto& part=catalog.parts.at(id);const auto& primitive=catalog.primitives[part.primitive];
    if(!part.bounds_prepared) {
      std::string error;
      if(!bounds_reader) {
        bounds_reader=std::make_unique<assets::GltfTriangleReader>();
        if(!bounds_reader->open(preparation_path(catalog.source),path.parent_path()/"scratch",error,cancel))
          throw std::runtime_error(error);
      }
      std::vector<std::uint64_t> order;if(!scene_order_triangles(path,catalog,part,order,error))throw std::runtime_error(error);
      if(order.empty()) {
        if(!bounds_reader->bounds(primitive.mesh,primitive.primitive,part.first_triangle,std::uint32_t(part.triangle_count),part.bounds,error))
          throw std::runtime_error(error);
      } else {
        assets::GltfTriangleWindow window;
        if(!bounds_reader->read(primitive.mesh,primitive.primitive,order,window,error))throw std::runtime_error(error);
        part.bounds=window.bounds;
      }
      check();part.bounds_prepared=true;modified=true;
    }
    checkpoint();report(ScenePreparationStage::Bounds,++completed,parts.size());
  }
  checkpoint(true);
}
}
