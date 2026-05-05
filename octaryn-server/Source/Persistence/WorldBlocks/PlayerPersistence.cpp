#include "WorldPersistence.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

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

bool read_text_file(const std::filesystem::path &path, std::string &payload) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
  return file.good() || file.eof();
}

bool write_text_file_atomically(const std::filesystem::path &path,
                                const std::string &payload) {
  const std::filesystem::path parent = path.parent_path();
  std::error_code error;
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      return false;
    }
  }

  const std::filesystem::path temp_path = path.string() + ".tmp";
  {
    std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      return false;
    }
    file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!file.good()) {
      std::filesystem::remove(temp_path, error);
      return false;
    }
  }

  std::filesystem::rename(temp_path, path, error);
  if (!error) {
    return true;
  }

  error.clear();
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temp_path, path, error);
  if (error) {
    std::filesystem::remove(temp_path, error);
    return false;
  }

  return true;
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
  if (!read_text_file(player_path, payload)) {
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

  return write_text_file_atomically(std::filesystem::path(path), output) ? 0
                                                                          : -3;
}

}
