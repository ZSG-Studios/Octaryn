#pragma once
#include "../MapWorld/MapModel.h"
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace octaryn::client::rendering::virtual_geometry {
inline constexpr std::uint32_t invalid_id=std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t page_bytes=65536,cluster_vertices=128,cluster_triangles=128;
inline constexpr std::uint32_t geometry_version=1;
struct GeometryBounds {float center[3]{},radius{},error{};};
struct GeometryCluster {
  std::uint32_t group{},refined_group{invalid_id},material{},page{};
  std::uint32_t vertex_offset{},vertex_count{},triangle_offset{},triangle_count{};
  // Low two bits: MapAlphaMode; bit 8: double sided.
  std::uint32_t flags{};
  GeometryBounds bounds;
};
struct GeometryGroup {
  std::uint32_t first_cluster{},cluster_count{},depth{},first_page{},page_count{};
  // FLT_MAX error identifies terminal groups; never project it to choose refinement.
  GeometryBounds simplified;
};
enum class GeometryCodec : std::uint32_t {Raw=0,Meshoptimizer=1};
enum class GeometrySpace : std::uint32_t {World=1,Object=2};
struct GeometryPage {
  std::uint64_t file_offset{};
  std::uint32_t encoded_size{},decoded_size{page_bytes};
  GeometryCodec codec{GeometryCodec::Raw};
  std::array<char,64> checksum{};
};
struct GeometryAsset {
  GeometrySpace space{GeometrySpace::World};
  // Static MapModel cooking remains world-space; animated assets retain object space.
  std::uint32_t material_count{};
  std::uint64_t source_triangles{};
  std::string source_hash;
  std::vector<GeometryCluster> clusters;
  std::vector<GeometryGroup> groups;
  std::vector<std::uint32_t> group_pages,roots;
  std::vector<GeometryPage> pages;
  // Optional encoded payloads. A manifest-only reader leaves this empty.
  std::vector<std::vector<std::uint8_t>> payloads;
};
static_assert(sizeof(GeometryBounds)==20 && sizeof(GeometryCluster)==56 && sizeof(GeometryGroup)==40);
bool validate_geometry(const GeometryAsset&,std::string& error);
bool decode_geometry_page(const GeometryAsset&,std::uint32_t page,std::vector<std::uint8_t>&,std::string& error);
}
