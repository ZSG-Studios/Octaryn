#pragma once
#include "LocalLight.h"
#include "SkyData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <vector>

namespace octaryn::client::rendering {
struct LightInfluence {
  std::array<float,3> minimum{},maximum{};
};
inline LightInfluence light_influence(const WorldLocalLight& light) {
  LightInfluence bounds;
  for(unsigned a=0;a<3;++a) {
    // Rectangles sample across both half axes; range alone misses their edges.
    const float extent=light.position_range[3]+(light.axis_v_type[3]==2.f?
      std::abs(light.axis_u_inner[a])+std::abs(light.axis_v_type[a]):0.f);
    bounds.minimum[a]=light.position_range[a]-extent;
    bounds.maximum[a]=light.position_range[a]+extent;
  }
  return bounds;
}
inline auto light_signature(const WorldLocalLight& light) {
  return std::tie(light.position_range,light.color_intensity,light.direction_outer,
    light.axis_u_inner,light.axis_v_type);
}
// Sorted multiset matching preserves unchanged sources through insertion,
// deletion and reordering, including multiple colocated lights. O(n log n).
class LocalLightChanges {
  std::vector<WorldLocalLight> previous_;
public:
  template<class Wake> void update(const std::vector<WorldLocalLight>& lights,Wake wake) {
    auto current=lights;
    const auto less=[](const auto& a,const auto& b){return light_signature(a)<light_signature(b);};
    std::sort(current.begin(),current.end(),less);
    std::size_t old=0,next=0;
    while(old<previous_.size() || next<current.size()) {
      if(next==current.size() || (old<previous_.size() && less(previous_[old],current[next])))
        wake(light_influence(previous_[old++]),true);
      else if(old==previous_.size() || less(current[next],previous_[old]))
        wake(light_influence(current[next++]),false);
      else {++old;++next;}
    }
    previous_=std::move(current);
  }
};
// Only radiometric inputs: camera, jitter, exposure and animation time are absent.
inline std::array<float,9> environment_signature(const SkyUniforms& sky,const SkyLighting& lighting) {
  const float sun=std::max(lighting.sun_strength,0.f);
  const float ambient=std::max(lighting.ambient_strength,0.f);
  // Match DDGIEnvironment's elevation-dependent tint, including twilight when
  // direct sun is off. Below that transition, an unlit sun's orbit adds no light.
  const float elevation=std::clamp((-sky.light_direction_sky[1]+.08f)/.28f,0.f,1.f);
  const float day=elevation*elevation*(3-2*elevation)*
    std::clamp(lighting.visual_sky_visibility,0.f,1.f);
  return {sky.light_direction_sky[0]*sun,sky.light_direction_sky[1]*sun,sky.light_direction_sky[2]*sun,
    sun,lighting.visual_sky_visibility,ambient,
    sky.twilight_celestial_time[0]*ambient,day*ambient,lighting.skylight_floor};
}
enum class EnvironmentChange { None, Gradual, Discontinuous };
class EnvironmentChanges {
  std::array<float,9> previous_{};
  double consumed_seconds_{};
  bool initialized_{};
public:
  EnvironmentChange update(const SkyUniforms& sky,const SkyLighting& lighting,double seconds) {
    const auto next=environment_signature(sky,lighting);
    if(!initialized_) {previous_=next;consumed_seconds_=seconds;initialized_=true;return EnvironmentChange::None;}
    float difference=0;
    bool discontinuous=false;
    for(unsigned i=0;i<next.size();++i) {
      const float delta=std::abs(next[i]-previous_[i]);
      difference=std::max(difference,delta);
      discontinuous|=delta>std::max(.025f,std::abs(previous_[i])*.1f);
    }
    if(difference<.0001f || (!discontinuous && seconds-consumed_seconds_<.25))return EnvironmentChange::None;
    // Accumulate throttled changes for classification. Consumers extend a
    // bounded tracking window instead of global dirty/history reset walks.
    previous_=next;consumed_seconds_=seconds;
    return discontinuous?EnvironmentChange::Discontinuous:EnvironmentChange::Gradual;
  }
};
struct LightingChanges {
  LocalLightChanges local;
  EnvironmentChanges environment;
  std::uint64_t light_revision{~0ull};
};
}
