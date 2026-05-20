#include "FrameLoopSupport.h"

#include "EmptyWorldMesh.h"
#include "Environment.h"
#include "FrameProfile.h"
#include "Log.h"
#include "Packing.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace octaryn_client_app {
namespace {

constexpr float kChunkWorldSize = 32.0f;
constexpr float kPerspectiveNearPlane = 0.5f;
constexpr float kRenderDistanceFarPlaneDiagonalScale = 1.5f;
constexpr float kRenderDistanceFarPlanePadding = 512.0f;
constexpr float kMovementProbeEyeHeight = 2.72f;
constexpr int kMovementProbeRenderDistance = 32;
constexpr const char *kMovementProbeFlag = "OCTARYN_CLIENT_APP_MOVEMENT_PROBE";
constexpr const char *kInputProbeFlag = "OCTARYN_CLIENT_APP_INPUT_PROBE";
constexpr const char *kPhaseProfileFlag = "OCTARYN_CLIENT_APP_PROFILE_PHASES";
constexpr const char *kProbeSpawnXEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_SPAWN_X";
constexpr const char *kProbeSpawnYEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_SPAWN_Y";
constexpr const char *kProbeSpawnZEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_SPAWN_Z";
constexpr const char *kProbeSpawnPitchEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_SPAWN_PITCH";
constexpr const char *kProbeSpawnYawEnv =
    "OCTARYN_CLIENT_APP_MOVEMENT_PROBE_SPAWN_YAW";

bool read_float_env(const char *name, float &value) {
  const char *raw = std::getenv(name);
  if (raw == nullptr || raw[0] == '\0') {
    return false;
  }
  char *end = nullptr;
  const float parsed = std::strtof(raw, &end);
  if (end == raw || !std::isfinite(parsed)) {
    return false;
  }
  value = parsed;
  return true;
}

bool movement_probe_has_explicit_spawn() {
  float ignored = 0.0f;
  return read_float_env(kProbeSpawnXEnv, ignored) &&
         read_float_env(kProbeSpawnYEnv, ignored) &&
         read_float_env(kProbeSpawnZEnv, ignored);
}

} // namespace

void apply_render_distance_far_plane(camera &camera, int render_distance) {
  const float distance_blocks =
      static_cast<float>(render_distance) * kChunkWorldSize;
  camera.near_plane = kPerspectiveNearPlane;
  camera.far_plane = distance_blocks * kRenderDistanceFarPlaneDiagonalScale +
                     kRenderDistanceFarPlanePadding;
}

void apply_movement_probe_render_distance(runtime_controls &controls) {
  if (!read_enabled_flag(kMovementProbeFlag) &&
      !read_enabled_flag(kInputProbeFlag)) {
    return;
  }

  controls.render_distance = kMovementProbeRenderDistance;
}

bool apply_movement_probe_camera_spawn(camera &camera) {
  if (!read_enabled_flag(kMovementProbeFlag)) {
    return false;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (!read_float_env(kProbeSpawnXEnv, x) ||
      !read_float_env(kProbeSpawnYEnv, y) ||
      !read_float_env(kProbeSpawnZEnv, z)) {
    return false;
  }

  camera.position[0] = x;
  camera.position[1] = y;
  camera.position[2] = z;
  read_float_env(kProbeSpawnPitchEnv, camera.pitch_radians);
  read_float_env(kProbeSpawnYawEnv, camera.yaw_radians);
  camera_update(&camera);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_movement_probe_spawn x=%.3f y=%.3f z=%.3f "
                 "pitch=%.6f yaw=%.6f\n",
                 camera.position[0], camera.position[1], camera.position[2],
                 camera.pitch_radians, camera.yaw_radians);
    std::fflush(g_log);
  }
  return true;
}

void align_movement_probe_camera_to_terrain(camera &camera,
                                            uint64_t frame_index) {
  if (!read_enabled_flag(kMovementProbeFlag) || frame_index != 1u) {
    return;
  }
  if (movement_probe_has_explicit_spawn()) {
    return;
  }

  const auto x = static_cast<int32_t>(std::floor(camera.position[0]));
  const auto z = static_cast<int32_t>(std::floor(camera.position[2]));
  int32_t surface_y = 0;
  for (int32_t dz = -1; dz <= 1; ++dz) {
    for (int32_t dx = -1; dx <= 1; ++dx) {
      const empty_world_terrain_column column =
          empty_world_seed_column(x + dx, z + dz);
      surface_y = std::max(surface_y, column.height);
    }
  }
  camera.position[1] = static_cast<float>(surface_y) + kMovementProbeEyeHeight;
  camera_update(&camera);
  if (g_log != nullptr && (frame_index == 1u || frame_index % 60u == 0u)) {
    std::fprintf(g_log,
                 "live_movement_probe_ground_lock frame=%" PRIu64
                 " block=(%d,%d) surface_y=%d eye_y=%.3f\n",
                 frame_index, x, z, surface_y, camera.position[1]);
    std::fflush(g_log);
  }
}

void log_camera_terrain_state(
    const camera &camera, const block_lookup &world_block_lookup,
    uint64_t frame_index) {
  if (g_log == nullptr || frame_index % 60u != 0u) {
    return;
  }
  constexpr float kPlayerEyeHeight = 1.62f;
  const auto x = static_cast<int32_t>(std::floor(camera.position[0]));
  const auto y = static_cast<int32_t>(std::floor(camera.position[1]));
  const auto z = static_cast<int32_t>(std::floor(camera.position[2]));
  const auto foot_y =
      static_cast<int32_t>(std::floor(camera.position[1] - kPlayerEyeHeight));
  const empty_world_terrain_column column = empty_world_seed_column(x, z);
  const uint16_t camera_block = empty_world_effective_block(
      world_block_lookup, block_position_key{x, y, z});
  const uint16_t foot_block = empty_world_effective_block(
      world_block_lookup, block_position_key{x, foot_y, z});
  const float eye_above_surface =
      camera.position[1] - (static_cast<float>(column.height) + 1.0f);
  std::fprintf(g_log,
               "live_camera_terrain_state frame=%" PRIu64
               " block=(%d,%d,%d) chunk=(%d,%d) surface_y=%d"
               " eye_above_surface=%.3f camera_block=%u foot_block=%u"
               " overrides=%zu\n",
               frame_index, x, y, z, floor_div_int32(x, kEmptyWorldChunkSize),
               floor_div_int32(z, kEmptyWorldChunkSize), column.height,
               eye_above_surface, static_cast<unsigned>(camera_block),
               static_cast<unsigned>(foot_block), world_block_lookup.size());
  std::fflush(g_log);
}

void log_frame_phase_profile(uint64_t frame_index, float controller_ms,
                             float terrain_align_ms, float raycast_ms,
                             float intent_ms, float poll_stream_ms,
                             float host_tick_ms, float mesh_update_ms,
                             float presentation_ms) {
  if (g_log == nullptr || !read_enabled_flag(kPhaseProfileFlag)) {
    return;
  }
  std::fprintf(g_log,
               "live_frame_phase_profile frame=%" PRIu64
               " controller_ms=%.3f terrain_align_ms=%.3f raycast_ms=%.3f"
               " intent_ms=%.3f poll_stream_ms=%.3f host_tick_ms=%.3f"
               " mesh_update_ms=%.3f presentation_ms=%.3f\n",
               frame_index, controller_ms, terrain_align_ms, raycast_ms,
               intent_ms, poll_stream_ms, host_tick_ms, mesh_update_ms,
               presentation_ms);
  std::fflush(g_log);
}

} // namespace octaryn_client_app
