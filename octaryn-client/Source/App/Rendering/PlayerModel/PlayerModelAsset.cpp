#include "PlayerModelAsset.h"

#include "Log.h"
#include "PlayerModelAnimation.h"
#include "PlayerModelData.h"

#include <SDL3/SDL.h>

#include <ozz/base/maths/simd_math.h>
#include <ozz/geometry/runtime/skinning_job.h>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace octaryn_client_app {
namespace {

constexpr float kEyeHeight = 1.62f;
constexpr float kAttackAnimationSeconds = 1.25f;
constexpr float kAnimationTransitionSeconds = 0.18f;

player_model_asset g_asset;

void reset_player_asset() {
  release_player_model_animation(g_asset);
  g_asset.vertices.clear();
  g_asset.parents.clear();
  g_asset.bind_pose.clear();
  g_asset.joints.clear();
  g_asset.inverse_bind.clear();
  g_asset.animations.clear();
  g_asset.loaded = false;
  g_asset.logged = false;
}

void register_player_asset_cleanup_once() {
  static bool registered = false;
  if (!registered) {
    std::atexit(reset_player_asset);
    registered = true;
  }
}

float movement_amount(const client_input_debug_state &input) {
  return std::min(1.0f, std::fabs(input.move_x) + std::fabs(input.move_z));
}

struct animation_selection {
  const char *name;
  float seconds;
};

animation_selection select_animation(const client_input_debug_state &input,
                                     float seconds) {
  static bool attack_was_down = false;
  static bool attack_armed = true;
  static float attack_start_seconds = -kAttackAnimationSeconds;
  const bool attack_down = (input.flags & kInputPrimaryFlag) != 0u;
  if (!attack_down &&
      seconds - attack_start_seconds >= kAttackAnimationSeconds) {
    attack_armed = true;
  }
  if (attack_down && !attack_was_down && attack_armed) {
    attack_start_seconds = seconds;
    attack_armed = false;
  }
  attack_was_down = attack_down;

  const float attack_seconds = seconds - attack_start_seconds;
  if (attack_seconds >= 0.0f && attack_seconds < kAttackAnimationSeconds) {
    return animation_selection{"attack_slash_once", attack_seconds};
  }

  if ((input.flags & kInputSecondaryFlag) != 0u) {
    return animation_selection{"wave_loop", seconds};
  }
  if (movement_amount(input) > 0.0f) {
    return animation_selection{
        (input.flags & kInputSprintFlag) != 0u ? "run_loop" : "walk_loop",
        seconds};
  }
  return animation_selection{"idle_loop", seconds};
}

bool skin_player_vertices(const std::vector<ozz::math::Float4x4> &skin,
                          std::vector<float> &positions,
                          std::vector<uint16_t> &joint_indices,
                          std::vector<float> &joint_weights,
                          std::vector<float> &skinned_positions) {
  positions.clear();
  joint_indices.clear();
  joint_weights.clear();
  skinned_positions.assign(g_asset.vertices.size() * 3u, 0.0f);
  positions.reserve(g_asset.vertices.size() * 3u);
  joint_indices.reserve(g_asset.vertices.size() * 4u);
  joint_weights.reserve(g_asset.vertices.size() * 3u);

  for (const player_source_vertex &source : g_asset.vertices) {
    positions.push_back(source.position[0]);
    positions.push_back(source.position[1]);
    positions.push_back(source.position[2]);
    for (size_t influence = 0; influence < 4u; ++influence) {
      joint_indices.push_back(source.joints[influence]);
    }
    for (size_t influence = 0; influence < 3u; ++influence) {
      joint_weights.push_back(source.weights[influence]);
    }
  }

  ozz::geometry::SkinningJob job{};
  job.vertex_count = static_cast<int>(g_asset.vertices.size());
  job.influences_count = 4;
  job.joint_matrices =
      ozz::span<const ozz::math::Float4x4>(skin.data(), skin.size());
  job.joint_indices =
      ozz::span<const uint16_t>(joint_indices.data(), joint_indices.size());
  job.joint_indices_stride = sizeof(uint16_t) * 4u;
  job.joint_weights =
      ozz::span<const float>(joint_weights.data(), joint_weights.size());
  job.joint_weights_stride = sizeof(float) * 3u;
  job.in_positions = ozz::span<const float>(positions.data(), positions.size());
  job.in_positions_stride = sizeof(float) * 3u;
  job.out_positions =
      ozz::span<float>(skinned_positions.data(), skinned_positions.size());
  job.out_positions_stride = sizeof(float) * 3u;
  return job.Run();
}

void append_skinned_vertex(const player_source_vertex &source,
                           const float *skinned, const player_fvec3 &origin,
                           float yaw, player_model_frame_vertices &frame) {
  const float yaw_sin = std::sin(yaw);
  const float yaw_cos = std::cos(yaw);
  player_model_render_vertex &vertex = frame.vertices[frame.count++];
  const float local_x = skinned[0];
  const float local_y = skinned[1];
  const float local_z = skinned[2];
  vertex.position[0] = origin[0] + local_x * yaw_cos - local_z * yaw_sin;
  vertex.position[1] = origin[1] + local_y;
  vertex.position[2] = origin[2] + local_x * yaw_sin + local_z * yaw_cos;
  vertex.position[3] = 1.0f;

  const float palette[3][4] = {
      {0.78f, 0.62f, 0.46f, 1.0f},
      {0.14f, 0.44f, 0.86f, 1.0f},
      {0.16f, 0.20f, 0.32f, 1.0f},
  };
  const uint32_t color = std::min(source.material, 2u);
  std::copy_n(palette[color], 4, vertex.color);
}

void log_model_bounds(const char *animation, uint64_t frame_index,
                      const std::vector<float> &positions) {
  static bool logged = false;
  if (g_log == nullptr || positions.empty()) {
    return;
  }
  float min_value[3] = {std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max()};
  float max_value[3] = {std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest()};
  for (size_t index = 0; index + 2u < positions.size(); index += 3u) {
    for (size_t axis = 0; axis < 3u; ++axis) {
      min_value[axis] = std::min(min_value[axis], positions[index + axis]);
      max_value[axis] = std::max(max_value[axis], positions[index + axis]);
    }
  }
  const float width = max_value[0] - min_value[0];
  const float height = max_value[1] - min_value[1];
  const float depth = max_value[2] - min_value[2];
  const bool stretched = width > 1.6f || height > 2.4f || depth > 1.6f;
  if (logged && frame_index % 120u != 0u && !stretched) {
    return;
  }
  std::fprintf(g_log,
               "live_player_model_bounds skinner=ozz_geometry frame=%" PRIu64
               " animation=%s min=(%.3f,%.3f,%.3f) "
               "max=(%.3f,%.3f,%.3f) extent=(%.3f,%.3f,%.3f) stretched=%d\n",
               frame_index, animation, min_value[0], min_value[1], min_value[2],
               max_value[0], max_value[1], max_value[2], width, height, depth,
               stretched ? 1 : 0);
  std::fflush(g_log);
  logged = true;
}

void log_asset_once(const player_model_asset &asset) {
  if (asset.logged || g_log == nullptr) {
    return;
  }
  std::fprintf(
      g_log,
      "live_player_model_asset active=%d source=octaryn_player_v1.gltf "
      "loader=fastgltf animator=ozz vertices=%zu joints=%zu "
      "animations=%zu\n",
      asset.loaded ? 1 : 0, asset.vertices.size(), asset.joints.size(),
      asset.animations.size());
  std::fflush(g_log);
  g_asset.logged = true;
}

bool ensure_asset_loaded() {
  register_player_asset_cleanup_once();
  if (g_asset.loaded) {
    return true;
  }
  if (!load_player_model_asset(g_asset)) {
    log_asset_once(g_asset);
    return false;
  }
  if (!initialize_player_model_animation(g_asset)) {
    g_asset.loaded = false;
    log_asset_once(g_asset);
    return false;
  }
  log_asset_once(g_asset);
  return true;
}

struct animation_transition {
  const char *from_name;
  float from_seconds;
  float weight;
};

animation_transition update_animation_transition(const char *name,
                                                 float seconds) {
  static const char *previous_name = "idle_loop";
  static float previous_seconds = 0.0f;
  static float transition_start_seconds = -kAnimationTransitionSeconds;
  static const char *transition_from_name = "idle_loop";
  static float transition_from_seconds = 0.0f;
  if (std::strcmp(previous_name, name) != 0) {
    transition_from_name = previous_name;
    transition_from_seconds = previous_seconds;
    transition_start_seconds = seconds;
    previous_name = name;
  }
  previous_seconds = seconds;
  const float elapsed = seconds - transition_start_seconds;
  const float weight = elapsed >= 0.0f && elapsed < kAnimationTransitionSeconds
                           ? 1.0f - elapsed / kAnimationTransitionSeconds
                           : 0.0f;
  return animation_transition{transition_from_name, transition_from_seconds,
                              weight};
}

float player_animation_seconds() {
  static uint64_t start_ticks = 0u;
  const uint64_t ticks = SDL_GetTicksNS();
  if (start_ticks == 0u) {
    start_ticks = ticks;
  }
  return static_cast<float>(static_cast<double>(ticks - start_ticks) * 1.0e-9);
}

} // namespace

bool build_player_model_frame(const camera &player_camera,
                              const client_input_debug_state &input,
                              const runtime_controls &controls,
                              uint64_t frame_index,
                              player_model_frame_vertices &frame) {
  if (!ensure_asset_loaded()) {
    return false;
  }

  frame = {};
  frame.asset_loaded = true;
  const float seconds = player_animation_seconds();
  const animation_selection animation = select_animation(input, seconds);
  frame.animation_name = animation.name;
  const animation_transition transition =
      update_animation_transition(animation.name, animation.seconds);

  std::vector<ozz::math::Float4x4> skin;
  if (!sample_player_model_animation_transition(
          g_asset, frame.animation_name, animation.seconds,
          transition.from_name, transition.from_seconds, transition.weight,
          skin)) {
    return false;
  }
  std::vector<float> source_positions;
  std::vector<uint16_t> joint_indices;
  std::vector<float> joint_weights;
  std::vector<float> skinned_positions;
  if (!skin_player_vertices(skin, source_positions, joint_indices,
                            joint_weights, skinned_positions)) {
    log_line("player_model_skinning=ozz_geometry_failed");
    return false;
  }
  log_model_bounds(frame.animation_name, frame_index, skinned_positions);

  const player_fvec3 origin(player_camera.position[0],
                            player_camera.position[1] - kEyeHeight,
                            player_camera.position[2]);
  for (size_t index = 0; index < g_asset.vertices.size(); ++index) {
    if (frame.count >= kMaxPlayerModelVertices) {
      break;
    }
    append_skinned_vertex(g_asset.vertices[index],
                          &skinned_positions[index * 3u], origin,
                          player_camera.yaw_radians, frame);
  }
  (void)controls;
  return frame.count > 0u;
}

} // namespace octaryn_client_app
