#pragma once
#include "MapImages.h"
#include <compare>
#include <vector>

namespace octaryn::client::rendering {
enum class MapMipRole { BaseColor,MetalRough,Normal,Occlusion,Emissive };
struct MapMipOptions {
  MapMipRole role{MapMipRole::BaseColor};
  bool alpha_weighted{};
  bool preserve_coverage{};
  float alpha_cutoff{.5f},alpha_factor{1};
  auto operator<=>(const MapMipOptions&) const = default;
};
// RGBA8, straight alpha. Color roles are encoded sRGB; other roles are linear.
std::vector<MapDecodedImage> build_map_mips(const MapDecodedImage&,const MapMipOptions&);
// Coverage targets include the material factor, not spatially varying vertex alpha.
MapMipOptions map_mip_options(const MapMaterial&,unsigned texture_role);
}
