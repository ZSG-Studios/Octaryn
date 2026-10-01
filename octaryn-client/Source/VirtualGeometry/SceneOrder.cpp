#include "SceneOrder.h"
#include "SceneResourceHash.h"
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
scene_geometry::SpatialOrderConfig scene_order_config(const SceneCatalog& catalog,const ScenePrimitive& primitive) {
  scene_geometry::SpatialOrderConfig config;config.source_hash=catalog.source_hash;config.mesh=primitive.mesh;
  config.primitive=primitive.primitive;config.triangles=primitive.triangles;config.bounds=primitive.bounds;
  config.part_triangles=catalog.part_triangles;return config;
}
bool verify_scene_orders(const std::filesystem::path& catalog_path,const SceneCatalog& catalog,std::string& error,const std::atomic_bool* cancel) {
  try {
    for(const auto& primitive:catalog.primitives)if(!primitive.triangle_order.empty()) {
      std::filesystem::path file;
      if(!scene_geometry::spatial_triangle_order_path(catalog_path,std::filesystem::u8path(primitive.triangle_order),file,error))return false;
      if(scene_resource_hash(file,error,cancel)!=primitive.triangle_order_hash)
        throw std::runtime_error(error.empty()?"scene triangle permutation content changed":error);
      if(!scene_geometry::validate_spatial_triangle_order(file,scene_order_config(catalog,primitive),error,cancel))return false;
    }
    error.clear();return true;
  }catch(const std::exception& failure){error=failure.what();return false;}
}
bool scene_order_triangles(const std::filesystem::path& catalog_path,const SceneCatalog& catalog,const ScenePart& part,
    std::vector<std::uint64_t>& result,std::string& error) {
  const auto& primitive=catalog.primitives.at(part.primitive);result.clear();
  if(primitive.triangle_order.empty()) {error.clear();return true;}
  std::filesystem::path file;
  if(!scene_geometry::spatial_triangle_order_path(catalog_path,std::filesystem::u8path(primitive.triangle_order),file,error))return false;
  return scene_geometry::read_spatial_triangle_order(file,
      scene_order_config(catalog,primitive),part.first_triangle,std::uint32_t(part.triangle_count),result,error);
}
}
