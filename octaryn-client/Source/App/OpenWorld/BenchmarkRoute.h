#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace octaryn::client::app {
inline bool benchmark_route_origin_valid(const std::array<float,3>& origin,
    double travel,unsigned radius) {
  if(!std::isfinite(travel) || travel<0 || travel>std::numeric_limits<float>::max())return false;
  auto endpoint=origin;
  endpoint[2]-=static_cast<float>(travel);
  // Signed block coordinates must contain the full window, preload and column alignment.
  const double padding=(double(radius)+2)*32;
  const double minimum=double(INT32_MIN)+padding,maximum=double(INT32_MAX)-padding;
  for(const auto& point:{origin,endpoint})for(float value:point)
    if(!std::isfinite(value) || double(value)<minimum || double(value)>maximum)return false;
  return true;
}
}
