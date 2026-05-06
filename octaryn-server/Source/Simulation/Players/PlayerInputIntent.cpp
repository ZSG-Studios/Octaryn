#include "PlayerSimulation.h"

#include <glaze/glaze.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace octaryn::server::simulation::players {

struct player_input_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  double deltaSeconds = 1.0 / 60.0;
  uint32_t flags = 0u;
  uint32_t controller = 0u;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float moveZ = 0.0f;
  float cameraX = 0.0f;
  float cameraY = 0.0f;
  float cameraZ = 0.0f;
  float cameraPitch = 0.0f;
  float cameraYaw = 0.0f;
  int32_t relativeMouse = 0;
};

} // namespace octaryn::server::simulation::players

namespace {

constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};

enum player_input_process_plan_reason : uint32_t {
  player_input_process_plan_reason_none = 0u,
  player_input_process_plan_reason_missing_intent = 1u,
  player_input_process_plan_reason_intent_read_retry = 2u,
  player_input_process_plan_reason_partial_intent = 3u,
  player_input_process_plan_reason_unsupported_intent = 4u,
  player_input_process_plan_reason_intent_read_failed = 5u,
};

using octaryn::server::simulation::players::player_input_intent_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool is_supported(const player_input_intent_file &file) {
  return file.version == 1 && file.frameIndex > 0u &&
         std::isfinite(file.deltaSeconds) && file.deltaSeconds >= 0.0;
}

OctarynServerPlayerInputIntent to_native_intent(
    const player_input_intent_file &file) {
  return OctarynServerPlayerInputIntent{
      .version = file.version,
      .frame_index = file.frameIndex,
      .delta_seconds = file.deltaSeconds,
      .input =
          OctarynServerPlayerInput{
              .flags = file.flags,
              .controller = file.controller,
              .move_x = file.moveX,
              .move_y = file.moveY,
              .move_z = file.moveZ,
              .camera_x = file.cameraX,
              .camera_y = file.cameraY,
              .camera_z = file.cameraZ,
              .camera_pitch = file.cameraPitch,
              .camera_yaw = file.cameraYaw,
              .relative_mouse = file.relativeMouse,
          },
  };
}

OctarynServerPlayerInputProcessPlan input_stop_plan(uint32_t reason,
                                                    int32_t handle_result) {
  return OctarynServerPlayerInputProcessPlan{
      .should_continue = handle_result == 0 ? 1u : 0u,
      .should_tick = 0u,
      .reason = reason,
      .handle_result = handle_result,
  };
}

const char *input_process_reason_name(uint32_t reason) {
  switch (reason) {
  case player_input_process_plan_reason_missing_intent:
    return "waiting_for_intent";
  case player_input_process_plan_reason_intent_read_retry:
    return "intent_read_retry";
  case player_input_process_plan_reason_partial_intent:
    return "partial_intent";
  case player_input_process_plan_reason_unsupported_intent:
    return "unsupported_intent";
  case player_input_process_plan_reason_intent_read_failed:
  default:
    return "intent_read_failed";
  }
}

} // namespace

extern "C" {

int32_t octaryn_server_player_read_input_intent_file(
    const char *intent_path, OctarynServerPlayerInputIntent *intent) {
  if (intent_path == nullptr || intent_path[0] == '\0' || intent == nullptr) {
    return -1;
  }

  const std::filesystem::path path{intent_path};
  if (!std::filesystem::exists(path)) {
    return 1;
  }

  std::string payload;
  if (!read_text_file(path, payload)) {
    return -2;
  }

  player_input_intent_file file{};
  if (glz::read<JsonReadOptions>(file, payload)) {
    return -3;
  }

  if (!is_supported(file)) {
    return -4;
  }

  *intent = to_native_intent(file);
  return 0;
}

int32_t octaryn_server_player_plan_input_intent(
    int32_t intent_read_result, uint32_t allow_transient_invalid,
    const OctarynServerPlayerInputIntent *intent,
    OctarynServerPlayerInputProcessPlan *plan) {
  if (plan == nullptr) {
    return -1;
  }

  const bool allow_transient = allow_transient_invalid != 0u;
  switch (intent_read_result) {
  case 0:
    break;
  case 1:
    *plan = input_stop_plan(player_input_process_plan_reason_missing_intent, 0);
    return 0;
  case -2:
    *plan = input_stop_plan(player_input_process_plan_reason_intent_read_retry,
                            allow_transient ? 0 : -1);
    return 0;
  case -3:
    *plan = input_stop_plan(player_input_process_plan_reason_partial_intent,
                            allow_transient ? 0 : -1);
    return 0;
  case -4:
    *plan =
        input_stop_plan(player_input_process_plan_reason_unsupported_intent, -1);
    return 0;
  default:
    *plan =
        input_stop_plan(player_input_process_plan_reason_intent_read_failed, -1);
    return 0;
  }

  if (intent == nullptr || intent->frame_index == 0u) {
    *plan =
        input_stop_plan(player_input_process_plan_reason_intent_read_failed, -1);
    return 0;
  }

  *plan = OctarynServerPlayerInputProcessPlan{
      .should_continue = 1u,
      .should_tick = 1u,
      .reason = player_input_process_plan_reason_none,
      .handle_result = 0,
  };
  return 0;
}

const char *octaryn_server_player_input_process_reason_name(uint32_t reason) {
  return input_process_reason_name(reason);
}

}
