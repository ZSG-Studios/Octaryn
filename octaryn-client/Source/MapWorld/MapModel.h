#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>

namespace octaryn::client::rendering {
// World-space vertex shared with MapGeometry.slang.
struct MapVertex {
  float position[3], normal[3], uv[2];
  float uv1[2]{},padding[2]{},tangent[4]{},color[4]{1,1,1,1};
};
static_assert(sizeof(MapVertex)==80);
enum class MapAlphaMode : std::uint32_t { Opaque=0,Mask=1,Blend=2 };
struct MapTexture {
  std::int32_t image{-1};
  std::uint32_t texcoord{},wrap_s{10497},wrap_t{10497},min_filter{9987},mag_filter{9729};
  float transform[6]{1,0,0,0,1,0};
};
struct MapMaterial {
  float base_color[4]{1,1,1,1};
  float metallic{1},roughness{1},alpha_cutoff{},normal_scale{1},occlusion_strength{1};
  float emissive[3]{};
  MapAlphaMode alpha_mode{MapAlphaMode::Opaque};
  bool double_sided{};
  std::int32_t texture{-1};
  // Base color, metallic/roughness, normal, occlusion, emissive.
  MapTexture textures[5];
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
struct MapLoadLimits {
  std::uint64_t source_bytes{512ull*1024*1024},encoded_bytes{512ull*1024*1024};
  std::size_t triangles{8000000},primitives{4096},accessor_elements{24000000};
  const std::atomic_bool* cancel{};
};
// Flattens the default scene of a .glb or .gltf+bin (+Y up) into world-space
// per-primitive vertex/index ranges with embedded or external images.
bool load_map_model(const std::filesystem::path&,MapModel&,std::string& error,const MapLoadLimits& limits={});
// Offline texture catalog uses the same parser/default scene/materials/images,
// but does not allocate or qualify mesh geometry.
bool load_map_texture_catalog(const std::filesystem::path&,MapModel&,std::string& error);
}
