#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace octaryn::client::rendering {
struct BlockTransportLighting {
  std::array<float,4> sun{},sky{};
};
inline BlockTransportLighting block_transport_lighting(std::array<float,4> sun,std::array<float,4> sky) {
  double squared=0;
  for(unsigned axis=0;axis<3;++axis)squared+=double(sun[axis])*double(sun[axis]);
  if(!std::isfinite(squared) || squared<=1e-12) {sun={0,1,0,0};}
  else {
    const auto inverse=1/std::sqrt(squared);
    for(unsigned axis=0;axis<3;++axis)sun[axis]=float(double(sun[axis])*inverse);
    sun[3]=std::isfinite(sun[3])?std::max(sun[3],0.f):0.f;
  }
  for(unsigned i=0;i<3;++i)sky[i]=std::isfinite(sky[i])?std::max(sky[i],0.f):0.f;
  sky[0]=std::min(sky[0],1.f);sky[2]=std::min(sky[2],1.f);sky[3]=0;
  return {sun,sky};
}
// Compare consecutive successfully submitted samples, not an accumulated anchor.
inline bool block_transport_lighting_discontinuity(const BlockTransportLighting& previous,
    const BlockTransportLighting& current) {
  const auto strength_changed=[](float before,float after) {
    if((before>0)!=(after>0))return true;
    return std::abs(double(after)-before)>=std::max(.05,.25*double(before));
  };
  if(strength_changed(previous.sun[3],current.sun[3]) ||
      strength_changed(previous.sky[1],current.sky[1]))return true;
  const bool environment=previous.sky[1]>0 || current.sky[1]>0;
  if(environment && (std::abs(double(current.sky[0])-previous.sky[0])>=.1 ||
      std::abs(double(current.sky[2])-previous.sky[2])>=.1))return true;
  if(environment || previous.sun[3]>0 || current.sun[3]>0) {
    double cosine=0;
    for(unsigned axis=0;axis<3;++axis)cosine+=double(previous.sun[axis])*current.sun[axis];
    constexpr double CosineThreeDegrees=.9986295347545738;
    if(cosine<CosineThreeDegrees)return true;
  }
  return false;
}
}
