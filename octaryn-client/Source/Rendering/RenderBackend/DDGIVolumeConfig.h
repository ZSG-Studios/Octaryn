#pragma once
#include "DDGISystem.h"
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
// Radius is in blocks. Farther coverage trades coarse probe density, not voxels.
inline DDGIConfig ddgi_volume_config(const DDGIConfig& base,unsigned radius,bool fine,float spacing) {
  DDGIConfig config=base;
  radius=std::min(radius,fine?32u:1024u);
  if(fine) {
    const unsigned side=std::clamp(radius*2,2u,64u);
    config.counts={side,side,side};config.spacing=1;
    config.budget=std::clamp(radius*8,32u,256u);
    config.max_distance=std::max(24.f,float(radius)*2.f);
  } else {
    const float minimum_spacing=std::isfinite(spacing)?std::clamp(spacing,.5f,32.f):8.f;
    config.spacing=std::max(minimum_spacing,float(radius*2)/32.f);
    const unsigned side=std::clamp(unsigned(std::ceil(float(radius*2)/config.spacing)),2u,32u);
    config.counts={side,12,side};
    config.budget=std::clamp(side*4,64u,256u);
    config.max_distance=std::max({96.f,float(radius)*2.f,config.spacing*12.f});
  }
  return config;
}
inline bool ddgi_same_config(const DDGIConfig& a,const DDGIConfig& b) {
  return a.counts==b.counts && a.spacing==b.spacing && a.hysteresis==b.hysteresis &&
    a.max_distance==b.max_distance && a.rays==b.rays && a.budget==b.budget &&
    a.irradiance_resolution==b.irradiance_resolution && a.visibility_resolution==b.visibility_resolution &&
    a.gpu_budget_milliseconds==b.gpu_budget_milliseconds;
}
inline void ddgi_set_gpu_budget(DDGISystem& s,double milliseconds) {
  if(s.config.gpu_budget_milliseconds==milliseconds)return;
  const double ratio=milliseconds/s.config.gpu_budget_milliseconds;
  s.config.gpu_budget_milliseconds=milliseconds;
  s.adaptive_budget*=ratio;s.budget_credit*=ratio;s.gpu_debt_seconds/=ratio;
}
}
