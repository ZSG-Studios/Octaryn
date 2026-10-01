#pragma once
#include "GeometryFormat.h"
#include <filesystem>

namespace octaryn::client::rendering::virtual_geometry {
struct MapGeometryCache {
  std::filesystem::path path;
  std::string hash;
  std::uint32_t pages{},clusters{},root_pages{};
};
// The key covers decoded geometry and final material coverage, including external buffers.
std::uint64_t map_geometry_reservation(const MapGeometryCache&);
bool prepare_map_geometry(const std::filesystem::path&,const MapModel&,MapGeometryCache&,std::string&);
}
