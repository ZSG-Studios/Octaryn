#include "WorldIntents.h"

#include "Environment.h"
#include "FileIO.h"
#include "Input.h"
#include "JsonContracts.h"
#include "Log.h"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace octaryn_client_app {

namespace {

constexpr glz::opts kJsonWriteOptions{.prettify = true};
constexpr uint32_t kMaxProcessChunkStreamRadius = 2u;

} // namespace

bool write_world_time_intent(const singleplayer_server_session &session,
                             const client_world_time_controls &controls) {
  if (!session.enabled) {
    return true;
  }

  client_world_time_intent_file intent{};
  intent.speedIndex = controls.speed_index;
  intent.speedMultiplier = controls.speed_multiplier;

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_world_time_intent_write=encode_failed");
    return false;
  }

  if (!write_text_file_atomic(session.world_time_intent_path, output,
                              "live_world_time_intent_write=failed")) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_world_time_intent source=process_file speed_index=%d "
                 "speed_multiplier=%.3f path=%s\n",
                 controls.speed_index, controls.speed_multiplier,
                 session.world_time_intent_path.string().c_str());
    std::fflush(g_log);
  }
  return true;
}

bool write_chunk_view_intent(const chunk_view &view,
                             const chunk_view &previous_view,
                             uint64_t epoch) {
  const char *path = std::getenv("OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    return true;
  }

  client_chunk_view_intent_file intent{};
  intent.epoch = epoch;
  intent.centerChunkX = view.origin_x + view.width / 2;
  intent.centerChunkZ = view.origin_z + view.width / 2;
  intent.radius = std::min(static_cast<uint32_t>(std::max(view.width / 2, 0)),
                           kMaxProcessChunkStreamRadius);
  if (previous_view.width > 0) {
    intent.hasPreviousWindow = true;
    intent.previousCenterChunkX = previous_view.origin_x + previous_view.width / 2;
    intent.previousCenterChunkZ = previous_view.origin_z + previous_view.width / 2;
    intent.previousRadius = std::min(
        static_cast<uint32_t>(std::max(previous_view.width / 2, 0)),
        kMaxProcessChunkStreamRadius);
  }

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_chunk_view_intent_write=encode_failed");
    return false;
  }

  if (!write_text_file_atomic(std::filesystem::path(path), output,
                              "live_chunk_view_intent_write=failed")) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_view_intent source=process_file path=%s "
                 "epoch=%" PRIu64 " center=(%d,%d) radius=%" PRIu32
                 " previous=%d previous_center=(%d,%d) previous_radius=%" PRIu32
                 "\n",
                 path, intent.epoch, intent.centerChunkX, intent.centerChunkZ,
                 intent.radius, intent.hasPreviousWindow ? 1 : 0,
                 intent.previousCenterChunkX, intent.previousCenterChunkZ,
                 intent.previousRadius);
    std::fflush(g_log);
  }
  return true;
}

bool write_player_input_intent(const octaryn_host_frame_snapshot &frame) {
  const char *path = std::getenv("OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    return true;
  }

  if (read_enabled_flag(kInputProbeFlag) && frame.timing.frame_index != 1u) {
    return true;
  }

  client_player_input_intent_file intent{};
  intent.frameIndex = frame.timing.frame_index;
  intent.deltaSeconds = frame.timing.delta_seconds;
  intent.flags = frame.input.flags;
  intent.controller = frame.input.controller;
  intent.moveX = frame.input.move_x;
  intent.moveY = frame.input.move_y;
  intent.moveZ = frame.input.move_z;
  intent.cameraX = frame.input.camera_x;
  intent.cameraY = frame.input.camera_y;
  intent.cameraZ = frame.input.camera_z;
  intent.cameraPitch = frame.input.camera_pitch;
  intent.cameraYaw = frame.input.camera_yaw;
  intent.relativeMouse = frame.input.relative_mouse;

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_player_input_intent_write=encode_failed");
    return false;
  }

  if (!write_text_file_atomic(std::filesystem::path(path), output,
                              "live_player_input_intent_write=failed")) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_player_input_intent source=process_file path=%s "
                 "frame=%" PRIu64 " flags=%" PRIu32 " controller=%" PRIu32
                 " move=(%.3f,%.3f,%.3f) "
                 "camera=(%.3f,%.3f,%.3f,%.6f,%.6f)\n",
                 path, frame.timing.frame_index, frame.input.flags,
                 frame.input.controller, frame.input.move_x, frame.input.move_y,
                 frame.input.move_z, frame.input.camera_x, frame.input.camera_y,
                 frame.input.camera_z, frame.input.camera_pitch,
                 frame.input.camera_yaw);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
