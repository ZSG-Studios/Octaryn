#pragma once

#include "PlayerModelAsset.h"

#include <fastgltf/core.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace octaryn_client_app {

using player_fmat4 = fastgltf::math::fmat4x4;
using player_fquat = fastgltf::math::fquat;
using player_fvec3 = fastgltf::math::fvec3;
using player_fvec4 = fastgltf::math::fvec4;
using player_u16vec4 = fastgltf::math::u16vec4;

struct player_source_vertex {
  player_fvec3 position{};
  player_u16vec4 joints{};
  player_fvec4 weights{};
  uint32_t material = 0u;
};

struct player_node_pose {
  player_fvec3 translation{};
  player_fquat rotation{};
  player_fvec3 scale{1.0f};
};

struct player_animation_channel {
  uint32_t node = 0u;
  fastgltf::AnimationPath path{};
  std::vector<float> times;
  std::vector<player_fvec3> vec3_values;
  std::vector<player_fquat> quat_values;
};

struct player_animation_clip {
  std::string name;
  float duration = 0.0f;
  std::vector<player_animation_channel> channels;
};

struct player_model_animation_runtime;

struct player_model_asset {
  player_model_asset();
  ~player_model_asset();

  bool loaded = false;
  bool logged = false;
  std::vector<player_source_vertex> vertices;
  std::vector<int32_t> parents;
  std::vector<player_node_pose> bind_pose;
  std::vector<uint32_t> joints;
  std::vector<player_fmat4> inverse_bind;
  std::vector<player_animation_clip> animations;
  std::unique_ptr<player_model_animation_runtime> animation_runtime;
};

bool load_player_model_asset(player_model_asset &out);

} // namespace octaryn_client_app
