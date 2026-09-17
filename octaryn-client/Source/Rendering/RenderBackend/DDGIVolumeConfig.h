#pragma once
#include "DDGISystem.h"
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
// Restore the independent block-radius mapping from the original RHI cascades.
inline DDGIConfig ddgi_volume_config(const DDGIConfig& base,unsigned radius,bool fine,float spacing) {
  DDGIConfig config=base;
  if(fine) {
    const unsigned side=std::clamp(radius*2,2u,64u);
    config.counts={side,side,side};config.spacing=1;
    config.budget=std::clamp(radius*8,32u,256u);
    config.max_distance=std::max(24.f,float(radius)*2.f);
  } else {
    config.spacing=std::max(spacing,float((radius*2+63)/64));
    const unsigned side=std::clamp(unsigned(std::ceil(float(radius*2)/config.spacing)),2u,64u);
    config.counts={side,12,side};
    config.budget=std::clamp(side*4,64u,256u);
    config.max_distance=std::max(96.f,config.spacing*12.f);
  }
  return config;
}
}
