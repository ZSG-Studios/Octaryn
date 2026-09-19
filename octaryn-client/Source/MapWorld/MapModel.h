#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::rendering {
// World-space vertex for the shared soup: 32 bytes, matching MapGeometry.slang.
struct MapVertex {
  float position[3], normal[3], uv[2];
};
static_assert(sizeof(MapVertex)==32);
enum class MapAlphaMode : std::uint32_t { Opaque=0,Mask=1,Blend=2 };
struct MapMaterial {
  float base_color[4]{1,1,1,1};
  float metallic{},roughness{1},alpha_cutoff{};
  MapAlphaMode alpha_mode{MapAlphaMode::Opaque};
  bool double_sided{};
  std::int32_t texture{-1};
};
struct MapPrimitive {
  std::uint32_t first_index{},index_count{};
  float bounds_min[3]{},bounds_max[3]{};
  MapMaterial material;
};
struct MapModelImage {
  std::vector<std::uint8_t> bytes;
  std::string mime_type;
};
struct MapModel {
  std::vector<MapVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<MapPrimitive> primitives;
  std::vector<MapModelImage> images;
};
// Flattens the default scene of a .glb or .gltf+bin (+Y up) into world-space
// per-primitive vertex/index ranges with embedded or external images.
bool load_map_model(const std::filesystem::path&,MapModel&,std::string& error);
}
