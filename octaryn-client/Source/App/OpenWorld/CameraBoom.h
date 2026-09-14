#pragma once
#include "CameraShoulder.h"
#include "WorldRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace octaryn::client::app {
namespace camera_detail {
inline float box_hit(const std::array<float,3>& eye,const std::array<float,3>& delta,
                     const std::array<int,3>& cell) {
  float near=0,far=1;
  for(std::size_t a=0;a<3;++a) {
    const float low=static_cast<float>(cell[a])-.18f;
    const float high=static_cast<float>(cell[a])+1.18f;
    if(std::abs(delta[a])<1e-6f) {
      if(eye[a]<low || eye[a]>high)return 1;
      continue;
    }
    float t0=(low-eye[a])/delta[a],t1=(high-eye[a])/delta[a];
    if(t0>t1)std::swap(t0,t1);
    near=std::max(near,t0);far=std::min(far,t1);
    if(near>far)return 1;
  }
  return far<0?1:std::clamp(near,0.0f,1.0f);
}
}
// Sweep the entire combined back/shoulder displacement, including lateral obstacles.
template<class Blocked>
rendering::WorldCamera shoulder_camera(rendering::WorldCamera result,CameraShoulder shoulder,Blocked&& blocked) {
  const float cp=std::cos(result.pitch),sy=std::sin(result.yaw),cy=std::cos(result.yaw);
  const float side=shoulder==CameraShoulder::Right?.75f:-.75f;
  const std::array<float,3> eye{result.x,result.y,result.z};
  const std::array<float,3> delta{-sy*cp*4+cy*side,-std::sin(result.pitch)*4,cy*cp*4+sy*side};
  std::array<int,3> low{},high{};
  for(std::size_t a=0;a<3;++a) {
    low[a]=static_cast<int>(std::floor(std::min(eye[a],eye[a]+delta[a])-.18f));
    high[a]=static_cast<int>(std::floor(std::max(eye[a],eye[a]+delta[a])+.18f));
  }
  float fraction=1;
  for(int z=low[2];z<=high[2];++z)for(int y=low[1];y<=high[1];++y)for(int x=low[0];x<=high[0];++x) {
    const float hit=camera_detail::box_hit(eye,delta,{x,y,z});
    if(hit<fraction && blocked(x,y,z))fraction=hit;
  }
  fraction=std::max(0.0f,fraction-.01f);
  result.x+=delta[0]*fraction;result.y+=delta[1]*fraction;result.z+=delta[2]*fraction;
  return result;
}
}
