#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace octaryn::assets {
struct GltfTriangleWindow {
  std::vector<float> positions;
  std::vector<std::uint32_t> indices;
  std::array<float,6> bounds{};
};
// A source window is object-space and has no renderer or material dependency.
// One decoded view at a time is staged to private mapped scratch, never the full scene.
class GltfTriangleReader {
public:
  GltfTriangleReader();
  ~GltfTriangleReader();
  bool open(const std::filesystem::path& source,const std::filesystem::path& scratch,
      std::string& error,const std::atomic_bool* cancel=nullptr);
  bool open_range(const std::filesystem::path& source,const std::filesystem::path& scratch,
      std::uint64_t offset,std::uint64_t length,std::string& error,const std::atomic_bool* cancel=nullptr);
  bool read(std::uint32_t mesh,std::uint32_t primitive,std::uint64_t first_triangle,
      std::uint32_t triangle_count,GltfTriangleWindow&,std::string& error);
  bool read(std::uint32_t mesh,std::uint32_t primitive,std::span<const std::uint64_t> triangles,
      GltfTriangleWindow&,std::string& error);
  bool bounds(std::uint32_t mesh,std::uint32_t primitive,std::uint64_t first_triangle,
      std::uint32_t triangle_count,std::array<float,6>&,std::string& error);
private:
  struct State;
  std::unique_ptr<State> state_;
};
bool transform_gltf_triangles(GltfTriangleWindow&,const std::array<float,16>& column_major,std::string& error);
}
