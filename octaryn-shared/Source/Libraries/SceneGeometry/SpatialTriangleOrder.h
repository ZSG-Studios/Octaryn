#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace octaryn::scene_geometry {
struct SpatialTriangle {
  std::array<float,3> centroid{};
  std::array<float,6> bounds{};
};
struct SpatialOrderConfig {
  std::string source_hash;
  std::uint32_t mesh{},primitive{},part_triangles{65536},run_triangles{65536},merge_width{8};
  std::uint64_t triangles{},maximum_scratch_bytes{4ull<<30};
  std::array<float,6> bounds{};
};
struct SpatialOrderPart {
  std::uint64_t first_triangle{},triangle_count{};
  std::array<float,6> bounds{};
};
struct SpatialOrderResult {
  std::vector<SpatialOrderPart> parts;
  std::uint64_t triangles{},peak_scratch_bytes{},peak_records{};
  std::uint32_t merge_passes{};
};
// The callback receives every original triangle once, in source order, in bounded windows.
using SpatialTriangleRead=std::function<bool(std::uint64_t,std::span<SpatialTriangle>,std::string&)>;
bool spatial_triangle_order_path(const std::filesystem::path& catalog,const std::filesystem::path& relative,
    std::filesystem::path& output,std::string& error);
// Produces a new source-bound permutation. Existing output is never overwritten.
bool write_spatial_triangle_order(const std::filesystem::path&,const SpatialOrderConfig&,const SpatialTriangleRead&,
    SpatialOrderResult&,std::string&,const std::atomic_bool* cancel=nullptr);
bool read_spatial_triangle_order(const std::filesystem::path&,const SpatialOrderConfig&,std::uint64_t first,
    std::uint32_t count,std::vector<std::uint64_t>&,std::string&);
// Exact coverage proof is bounded to a 64MiB bitmap and 512KiB read window.
bool validate_spatial_triangle_order(const std::filesystem::path&,const SpatialOrderConfig&,std::string&,
    const std::atomic_bool* cancel=nullptr);
}
