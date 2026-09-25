#include "WorldPersistence.h"

#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>

namespace octaryn::server::persistence {

struct world_time_save_file {
  uint32_t version = 1u;
  uint64_t day_index = 0u;
  double seconds_of_day = 0.0;
};

} // namespace octaryn::server::persistence

namespace {

using octaryn::server::persistence::world_time_save_file;

constexpr uint32_t WorldTimeVersion = 1u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts JsonWriteOptions{.prettify = true};

world_time_save_file file_from_state(
    const octaryn_server_persistence_world_time_state &state) {
  return world_time_save_file{
      .version = state.version,
      .day_index = state.day_index,
      .seconds_of_day = state.seconds_of_day,
  };
}

octaryn_server_persistence_world_time_state state_from_file(
    const world_time_save_file &file) {
  return octaryn_server_persistence_world_time_state{
      .version = file.version,
      .day_index = file.day_index,
      .seconds_of_day = file.seconds_of_day,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_world_time_file(
    const char *path, octaryn_server_persistence_world_time_state *state) {
  if (path == nullptr || path[0] == '\0' || state == nullptr) {
    return -1;
  }

  const std::filesystem::path world_time_path(path);
  if (!std::filesystem::exists(world_time_path)) {
    return 1;
  }

  std::string payload;
  if (!octaryn::server::persistence::read_text_file(world_time_path, payload)) {
    return -2;
  }

  world_time_save_file file{};
  if (glz::read<JsonReadOptions>(file, payload) ||
      file.version != WorldTimeVersion) {
    return -3;
  }

  *state = state_from_file(file);
  return 0;
}

int32_t octaryn_server_persistence_write_world_time_file(
    const char *path,
    const octaryn_server_persistence_world_time_state *state) {
  if (path == nullptr || path[0] == '\0' || state == nullptr ||
      state->version != WorldTimeVersion) {
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
