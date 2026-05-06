#include "WorldPersistence.h"

#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <limits>
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

world_metadata_file
file_from_metadata(const octaryn_server_persistence_world_metadata &metadata) {
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

octaryn_server_persistence_world_metadata
metadata_from_file(const world_metadata_file &file) {
  return octaryn_server_persistence_world_metadata{
      .save_exists = file.save_exists ? 1u : 0u,
      .has_world_time = file.has_world_time ? 1u : 0u,
      .has_player_data = file.has_player_data ? 1u : 0u,
      .has_world_data = file.has_world_data ? 1u : 0u,
      .player_count = file.player_count,
      .chunk_override_count = file.chunk_override_count,
  };
}

int32_t to_count(uint32_t value, int32_t &count) {
  if (value > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return -1;
  }

  count = static_cast<int32_t>(value);
  return 0;
}

using root_child_path_reader = int32_t (*)(const char *, char *, uint64_t,
                                           uint64_t *);

std::filesystem::path root_child_path(const char *world_root,
                                      root_child_path_reader read_path) {
  uint64_t required_size = 0;
  if (read_path(world_root, nullptr, 0, &required_size) != 0 ||
      required_size == 0u) {
    return {};
  }

  std::string path(required_size, '\0');
  uint64_t written_size = 0;
  if (read_path(world_root, path.data(), required_size, &written_size) != 0 ||
      written_size != required_size) {
    return {};
  }

  return std::filesystem::path(path.c_str());
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

int32_t octaryn_server_persistence_build_world_metadata(
    const char *world_root,
    octaryn_server_persistence_world_metadata *metadata) {
  if (world_root == nullptr || world_root[0] == '\0' || metadata == nullptr) {
    return -1;
  }

  const std::filesystem::path world_time_path = root_child_path(
      world_root, octaryn_server_persistence_world_time_path_for_root);
  if (world_time_path.empty()) {
    return -1;
  }

  octaryn_server_persistence_world_time_state world_time{};
  const bool has_world_time =
      octaryn_server_persistence_read_world_time_file(
          world_time_path.string().c_str(), &world_time) == 0;

  uint32_t player_count_raw = 0u;
  int32_t result = octaryn_server_persistence_read_player_directory_count(
      world_root, &player_count_raw);
  if (result != 0) {
    return result;
  }

  octaryn_server_persistence_chunk_override_directory_scan scan{};
  result = octaryn_server_persistence_scan_chunk_override_directory(world_root,
                                                                    "", &scan);
  if (result != 0) {
    return result;
  }

  uint32_t chunk_override_count_raw = scan.file_count;
  if (chunk_override_count_raw == 0u) {
    const std::filesystem::path world_blocks_path = root_child_path(
        world_root,
        octaryn_server_persistence_world_block_override_path_for_root);
    if (world_blocks_path.empty()) {
      return -1;
    }

    result = octaryn_server_persistence_count_world_block_override_columns(
        world_blocks_path.string().c_str(), &chunk_override_count_raw);
    if (result != 0) {
      return result;
    }
  }

  int32_t player_count = 0;
  int32_t chunk_override_count = 0;
  if (to_count(player_count_raw, player_count) != 0 ||
      to_count(chunk_override_count_raw, chunk_override_count) != 0) {
    return -2;
  }

  *metadata = octaryn_server_persistence_world_metadata{
      .save_exists =
          has_world_time || player_count > 0 || chunk_override_count > 0 ? 1u
                                                                         : 0u,
      .has_world_time = has_world_time ? 1u : 0u,
      .has_player_data = player_count > 0 ? 1u : 0u,
      .has_world_data = chunk_override_count > 0 ? 1u : 0u,
      .player_count = player_count,
      .chunk_override_count = chunk_override_count,
  };
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
