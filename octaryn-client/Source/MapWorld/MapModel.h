#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <span>
#include "MapSceneLighting.h"

namespace octaryn::client::rendering {
// World-space vertex shared with MapGeometry.slang.
struct MapVertex {
  float position[3], normal[3], uv[2];
  float uv1[2]{},padding[2]{},tangent[4]{},color[4]{1,1,1,1};
  float blend0[4]{1,0,0,0},blend1[4]{};
};
static_assert(sizeof(MapVertex)==112);
enum class MapAlphaMode : std::uint32_t { Opaque=0,Mask=1,Blend=2 };
struct MapTexture {
  std::int32_t image{-1};
  std::uint32_t texcoord{},wrap_s{10497},wrap_t{10497},min_filter{9987},mag_filter{9729};
  float transform[6]{1,0,0,0,1,0};
};
struct MapMaterial {
  float base_color[4]{1,1,1,1};
  float blend0[4]{1,0,0,0},blend1[4]{};
  float metallic{1},roughness{1},alpha_cutoff{},normal_scale{1},occlusion_strength{1};
  float emissive[3]{};
  MapAlphaMode alpha_mode{MapAlphaMode::Opaque};
  bool double_sided{};
  bool unlit{};
  bool additive{},view_fade{};
  bool zero_basis{};
  std::array<float,4> view_fade_parameters{0,1,1,1};
  std::int32_t texture{-1};
  // Base color, metallic/roughness, normal, occlusion, emissive.
  std::uint32_t layer_count{};
  // Standard five roles followed by eight diffuse/normal layer pairs.
  MapTexture textures[21];
};
struct MapPrimitiveSource {
  std::uint32_t node{UINT32_MAX},mesh{UINT32_MAX},primitive{UINT32_MAX};
  std::string node_name,mesh_name;
  std::array<float,16> evaluated{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  std::array<float,6> local_bounds{};
};
struct MapPrimitive {
  bool collision{true};
  std::uint32_t first_index{},index_count{};
  float bounds_min[3]{},bounds_max[3]{};
  MapMaterial material;
  MapPrimitiveSource source;
};
struct MapModelImage {
  std::vector<std::uint8_t> bytes;
  std::string mime_type;
};
struct MapModel {
  MapSceneEnvironment environment;
  std::vector<WorldLocalLight> lights;
  std::vector<MapVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<std::uint32_t> collision_indices;
  std::vector<MapPrimitive> primitives;
  std::vector<MapModelImage> images;
};
struct MapLoadLimits {
  std::uint64_t source_bytes{512ull*1024*1024},encoded_bytes{512ull*1024*1024};
  std::size_t triangles{8000000},primitives{4096},accessor_elements{24000000};
  const std::atomic_bool* cancel{};
  std::uint64_t geometry_bytes{512ull*1024*1024};
  std::uint64_t source_offset{},source_length{};
  std::span<const std::string> excluded_nodes;
};
// Flattens the default scene of a .glb or .gltf+bin (+Y up) into world-space
// per-primitive vertex/index ranges with embedded or external images.
bool load_map_model(const std::filesystem::path&,MapModel&,std::string& error,const MapLoadLimits& limits={});
// Offline texture catalog uses the same parser/default scene/materials/images,
// but does not allocate or qualify mesh geometry.
bool load_map_texture_catalog(const std::filesystem::path&,MapModel&,std::string& error);
// Loads only images referenced by these materials; never decodes geometry or expands nodes.
bool load_map_material_resources(const std::filesystem::path&,std::span<const MapMaterial>,
    MapModel&,std::string& error,const MapLoadLimits& limits={});
}
