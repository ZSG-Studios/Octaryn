#pragma once
#include "SceneCatalog.h"
#include "SpatialTriangleOrder.h"
#include <functional>

namespace octaryn::client::rendering {
class MapSourceReader;
namespace virtual_geometry {
scene_geometry::SpatialOrderConfig scene_order_config(const SceneCatalog&,const ScenePrimitive&);
bool verify_scene_orders(const std::filesystem::path& catalog,const SceneCatalog&,std::string&,const std::atomic_bool* cancel=nullptr);
bool scene_order_triangles(const std::filesystem::path& catalog,const SceneCatalog&,const ScenePart&,
    std::vector<std::uint64_t>&,std::string&);
bool load_scene_part(MapSourceReader&,const std::filesystem::path& catalog,const SceneCatalog&,const ScenePart&,MapModel&,std::string&);
bool prepare_scene_spatial_order(const std::filesystem::path& catalog,std::uint64_t first_primitive,std::uint64_t primitive_count,
    std::string& error,const std::atomic_bool* cancel=nullptr,bool multipart_only=false,
    std::function<void(std::uint64_t completed,std::uint64_t total)> notify={});
}
}
