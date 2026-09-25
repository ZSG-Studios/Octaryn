#pragma once
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
inline float temporal_scale(float value,float fallback) {
  return std::isfinite(value)?std::clamp(value,1.f/3.f,1.f):fallback;
}
inline float temporal_preset_scale(unsigned mode,float custom) {
  constexpr float scales[]={1,1,1.f/1.5f,1.f/1.7f,.5f,1.f/3.f};
  return mode<6?scales[mode]:temporal_scale(custom,.667f);
}
// GPU-only feedback. Invalid samples cannot move resolution or enter the filter.
struct TemporalResolution {
  float scale{1},minimum{.5f},maximum{1},average_ms{};
  unsigned target_fps{60},samples{};
  bool active{};
  void configure(unsigned mode,float custom,bool enabled,float low,float high,unsigned fps) {
    minimum=temporal_scale(low,.5f);maximum=std::max(minimum,temporal_scale(high,1));
    target_fps=std::clamp(fps,30u,240u);active=enabled && mode>=2;
    scale=temporal_preset_scale(mode,custom);
    if(active)scale=std::clamp(scale,minimum,maximum);
    average_ms=0;samples=0;
  }
  bool sample(float gpu_ms) {
    if(!active || !std::isfinite(gpu_ms) || gpu_ms<=0 || gpu_ms>1000)return false;
    average_ms=samples?average_ms*.9f+gpu_ms*.1f:gpu_ms;
    if(++samples<12 || samples%8)return false;
    const float budget=1000.f/float(target_fps)*.95f;
    if(average_ms>=budget*.94f && average_ms<=budget*1.04f)return false;
    const float desired=scale*std::sqrt(budget/average_ms);
    const float next=std::clamp(desired,scale-.025f,scale+.01f);
    const float bounded=std::clamp(next,minimum,maximum);
    if(bounded==scale || (std::abs(bounded-scale)<.001f && bounded!=minimum && bounded!=maximum))return false;
    scale=bounded;return true;
  }
};
}
