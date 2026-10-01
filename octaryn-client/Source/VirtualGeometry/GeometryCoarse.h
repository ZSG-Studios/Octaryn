#pragma once
#include "GeometryFormat.h"
#include <atomic>
#include <filesystem>

namespace octaryn::client::rendering::virtual_geometry {
struct GeometryCoarseOptions {
  bool position_only{};
  std::uint32_t target_triangles{2048},maximum_triangles{262144};
  float inherited_error{},maximum_error{1000000};
  const std::atomic_bool* cancel{};
};
// A complete simplified representation, with no descendant metadata or hidden leaf pages.
bool cook_coarse_geometry(const MapModel&,const std::string&,GeometryAsset&,std::string&,GeometryCoarseOptions={});
bool append_geometry_roots(const std::filesystem::path&,const GeometryAsset&,MapModel&,
    std::uint64_t maximum_triangles,std::string&,const std::atomic_bool* cancel=nullptr);
std::uint64_t geometry_metadata_bytes(const GeometryAsset&);
}
