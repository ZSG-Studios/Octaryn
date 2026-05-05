#include "WorldPersistence.h"

#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>

namespace octaryn::server::persistence {

struct world_metadata_file {
  uint32_t version = 1u;
  bool save_exists = false;
  bool has_world_time = false;
  bool has_player_data = false;
  bool has_world_data = false;
  int32_t player_count = 0;
  int32_t chunk_override_count = 0;
};

} // namespace octaryn::server::persistence

namespace {

using octaryn::server::persistence::world_metadata_file;

constexpr uint32_t WorldMetadataVersion = 1u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts JsonWriteOptions{.prettify = true};

world_metadata_file file_from_metadata(
    const octaryn_server_persistence_world_metadata &metadata) {
  return world_metadata_file{
      .version = WorldMetadataVersion,
      .save_exists = metadata.save_exists != 0u,
      .has_world_time = metadata.has_world_time != 0u,
      .has_player_data = metadata.has_player_data != 0u,
      .has_world_data = metadata.has_world_data != 0u,
      .player_count = metadata.player_count,
      .chunk_override_count = metadata.chunk_override_count,
  };
}

octaryn_server_persistence_world_metadata metadata_from_file(
    const world_metadata_file &file) {
  return octaryn_server_persistence_world_metadata{
      .save_exists = file.save_exists ? 1u : 0u,
      .has_world_time = file.has_world_time ? 1u : 0u,
      .has_player_data = file.has_player_data ? 1u : 0u,
      .has_world_data = file.has_world_data ? 1u : 0u,
      .player_count = file.player_count,
      .chunk_override_count = file.chunk_override_count,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_world_metadata_file(
    const char *path, octaryn_server_persistence_world_metadata *metadata) {
  if (path == nullptr || path[0] == '\0' || metadata == nullptr) {
    return -1;
  }

  const std::filesystem::path metadata_path(path);
  if (!std::filesystem::exists(metadata_path)) {
    return 1;
  }

  std::string payload;
  if (!octaryn::server::persistence::read_text_file(metadata_path, payload)) {
    return -2;
  }

  world_metadata_file file{};
  if (glz::read<JsonReadOptions>(file, payload) ||
      file.version != WorldMetadataVersion) {
    return -3;
  }

  *metadata = metadata_from_file(file);
  return 0;
}

int32_t octaryn_server_persistence_write_world_metadata_file(
    const char *path,
    const octaryn_server_persistence_world_metadata *metadata) {
  if (path == nullptr || path[0] == '\0' || metadata == nullptr) {
    return -1;
  }

  std::string output;
  if (glz::write<JsonWriteOptions>(file_from_metadata(*metadata), output)) {
    return -2;
  }

  return octaryn::server::persistence::write_text_file_atomically(
             std::filesystem::path(path), output)
             ? 0
             : -3;
}

}
