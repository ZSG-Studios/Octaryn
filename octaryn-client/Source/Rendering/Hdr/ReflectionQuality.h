#pragma once
#include <array>

namespace octaryn::client::rendering {
struct ReflectionQuality {
  unsigned divisor, directions, fresh_samples, history_frames;
};
// Lower tiers change only reflection sampling; scene and presentation stay native.
inline constexpr ReflectionQuality reflection_quality(unsigned tier) {
  constexpr std::array<ReflectionQuality,4> tiers{{
    {3,2,1,16}, {2,2,1,24}, {2,4,1,32}, {1,8,2,32}}};
  return tiers[tier<tiers.size()?tier:2];
}
inline constexpr unsigned reflection_extent(unsigned source,unsigned divisor) {
  return source?1+(source-1)/divisor:1;
}
inline constexpr std::array<float,4> reflection_sampling(unsigned tier) {
  const auto quality=reflection_quality(tier);
  return {float(quality.directions),float(quality.fresh_samples),float(quality.history_frames),0};
}
static_assert(reflection_extent(2560,reflection_quality(2).divisor)==1280);
static_assert(reflection_extent(1441,reflection_quality(2).divisor)==721);
static_assert(reflection_extent(2560,reflection_quality(3).divisor)==2560);
static_assert(reflection_quality(2).directions==4 && reflection_quality(2).fresh_samples==1);
}
