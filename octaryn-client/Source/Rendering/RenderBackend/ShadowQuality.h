#pragma once
namespace octaryn::client::rendering {
struct ShadowQualityPolicy {
  unsigned samples, raster_resolution, filter_radius;
};
constexpr ShadowQualityPolicy shadow_quality_policy(unsigned tier) {
  switch(tier) {
    case 0:return {1,512,0};
    case 1:return {2,1024,1};
    case 3:return {8,2048,2};
    default:return {4,1024,1};
  }
}
}
