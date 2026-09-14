#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
enum class PlayerClip : uint32_t { Idle, Walk, Run, Crouch, Jump, Fall, Attack, Wave };
struct PlayerPose {
  float feet_x{}, feet_y{}, feet_z{}, yaw{};
  double source_seconds{};
  PlayerClip clip{PlayerClip::Idle};
  uint64_t action_sequence{};
  bool first_person{true}, visible{true};
};
struct PlayerLighting {
  float light_direction[3]{0,1,0};
  float ambient{0.35f}, sun_strength{0.65f};
};
}
