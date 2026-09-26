#pragma once
namespace octaryn::client::rendering {
struct ShadowQualityPolicy {
  unsigned samples, raster_resolution, filter_radius;
};
constexpr ShadowQualityPolicy shadow_quality_policy(unsigned tier) {
  switch(tier) {
    case 0:return {2,512,0};
    case 1:return {4,1024,1};
    case 3:return {8,2048,2};
    // High defaults to the full eight-tap disk: penumbrae need the extra levels
    // to stay smooth while the sun drifts and history resets.
    default:return {8,1024,1};
  }
}
}
