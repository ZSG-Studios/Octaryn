#pragma once
#include "MapModel.h"
#include <algorithm>
#include <cstddef>
namespace octaryn::client::rendering {
// Matches MapGeometry.slang; padding[0] is first triangle, [1] is unlit.
struct MapRayMaterial {
  float base_color[4]{1,1,1,1};
  float emissive[3]{},normal_scale{1};
  float metallic{1},roughness{1},alpha_cutoff{},occlusion_strength{1};
  std::uint32_t alpha_mode{},double_sided{},padding[2]{};
  struct Texture {
    std::uint64_t image{},sampler{};
    float transform[6]{1,0,0,0,1,0};
    std::uint32_t texcoord{},present{};
  } textures[21];
};
static_assert(sizeof(MapRayMaterial)==1072);
static_assert(offsetof(MapRayMaterial,padding)==56 && offsetof(MapRayMaterial,textures)==64);
inline MapRayMaterial map_material_record(const MapMaterial& source,std::uint32_t first_triangle) {
  MapRayMaterial record;
  std::copy_n(source.base_color,4,record.base_color);std::copy_n(source.emissive,3,record.emissive);
  record.normal_scale=source.normal_scale;record.metallic=source.metallic;record.roughness=source.roughness;
  record.alpha_cutoff=source.alpha_cutoff;record.occlusion_strength=source.occlusion_strength;
  record.alpha_mode=static_cast<unsigned>(source.alpha_mode);record.double_sided=source.double_sided?1u:0u;
  record.padding[0]=first_triangle;record.padding[1]=(source.unlit?1u:0u)|(source.zero_basis?2u:0u)|(source.layer_count<<8);
  return record;
}
}
