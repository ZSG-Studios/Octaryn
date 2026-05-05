#include "WorldPersistence.h"

#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>

namespace octaryn::server::persistence {

struct player_save_file {
  uint32_t version = 1u;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  uint16_t block = 0u;
};

} // namespace octaryn::server::persistence

namespace {

using octaryn::server::persistence::player_save_file;

constexpr uint32_t PlayerSaveVersion = 1u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts JsonWriteOptions{.prettify = true};

player_save_file file_from_state(
    const octaryn_server_persistence_player_state &state) {
  return player_save_file{
      .version = PlayerSaveVersion,
      .x = state.x,
      .y = state.y,
      .z = state.z,
      .pitch = state.pitch,
      .yaw = state.yaw,
      .block = state.block,
  };
}

octaryn_server_persistence_player_state state_from_file(
    const player_save_file &file) {
  return octaryn_server_persistence_player_state{
      .x = file.x,
      .y = file.y,
      .z = file.z,
      .pitch = file.pitch,
      .yaw = file.yaw,
      .block = file.block,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_player_file(
    const char *path, octaryn_server_persistence_player_state *state) {
  if (path == nullptr || path[0] == '\0' || state == nullptr) {
    return -1;
  }

  const std::filesystem::path player_path(path);
  if (!std::filesystem::exists(player_path)) {
    return 1;
  }

  std::string payload;
  if (!octaryn::server::persistence::read_text_file(player_path, payload)) {
    return -2;
  }

  player_save_file file{};
  if (glz::read<JsonReadOptions>(file, payload) ||
      file.version != PlayerSaveVersion) {
    return -3;
  }

  *state = state_from_file(file);
  return 0;
}

int32_t octaryn_server_persistence_write_player_file(
    const char *path, const octaryn_server_persistence_player_state *state) {
  if (path == nullptr || path[0] == '\0' || state == nullptr) {
    return -1;
  }

  std::string output;
  if (glz::write<JsonWriteOptions>(file_from_state(*state), output)) {
    return -2;
  }

  return octaryn::server::persistence::write_text_file_atomically(
             std::filesystem::path(path), output)
             ? 0
             : -3;
}

}
