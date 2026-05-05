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

}
