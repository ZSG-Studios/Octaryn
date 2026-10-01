#pragma once
#include "CharacterMotion.h"
#include "CharacterMotionLimits.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::character_motion {
// Covers the 1.8m capsule, support/step queries, and a preload margin around its eye.
inline constexpr float CharacterCollisionRadius=3.f;
inline float character_collision_radius(const State& state,const Input& input,float seconds) {
  const float dt=std::isfinite(seconds)?std::clamp(seconds,0.f,.25f):0;
  for(float value:{state.velocity_x,state.velocity_y,state.velocity_z,input.move_x,input.move_y,input.move_z})
    if(!std::isfinite(value))return std::numeric_limits<float>::infinity();
  if(input.flags&4u) {
    const float speed=input.flags&2u?SprintFlySpeedBlocksPerSecond:NormalFlySpeedBlocksPerSecond;
    // Camera-relative forward and vertical input can align; the L1 bound covers that case.
    const auto clamp_distance=std::abs(state.y-std::clamp(state.y,MinimumFlyHeight,MaximumFlyHeight));
    return CharacterCollisionRadius+clamp_distance+speed*dt*(std::abs(input.move_x)+std::abs(input.move_y)+std::abs(input.move_z));
  }
  const float target=input.flags&2u?SprintWalkSpeedBlocksPerSecond:WalkSpeedBlocksPerSecond;
  const float horizontal=std::max(std::hypot(state.velocity_x,state.velocity_z),target);
  const float vertical=std::max(std::abs(state.velocity_y),input.flags&1u?JumpSpeed:0.f);
  return CharacterCollisionRadius+std::hypot(horizontal,vertical)*dt+Gravity*dt*dt;
}
}
