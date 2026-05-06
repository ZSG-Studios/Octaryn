#include "WorldPersistence.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

constexpr const char *WorldBlocksPathEnv = "OCTARYN_SERVER_WORLD_BLOCKS_PATH";
constexpr const char *PlayerSaveRootEnv = "OCTARYN_SERVER_PLAYER_SAVE_ROOT";
constexpr const char *BuildPresetEnv = "OctarynBuildPresetName";
constexpr const char *DefaultBuildPreset = "debug-linux";

bool has_value(const char *value) {
  return value != nullptr && value[0] != '\0';
}

std::filesystem::path default_world_root(const char *preset) {
  if (!has_value(preset)) {
    preset = DefaultBuildPreset;
  }
  return std::filesystem::path("build") / preset / "server" / "world";
}

std::filesystem::path world_root_path(const char *world_blocks_path,
                                      const char *preset) {
  if (has_value(world_blocks_path)) {
    const std::filesystem::path root =
        std::filesystem::path(world_blocks_path).parent_path();
    if (!root.empty()) {
      return root;
    }
  }

  return default_world_root(preset);
}

std::filesystem::path world_block_override_path(const char *world_blocks_path,
                                                const char *preset) {
  if (has_value(world_blocks_path)) {
    return std::filesystem::path(world_blocks_path);
  }

  return default_world_root(preset) / "world_blocks.json";
}

std::filesystem::path player_directory_path(const char *player_save_root,
                                            const char *world_blocks_path,
                                            const char *preset) {
  if (has_value(player_save_root)) {
    return std::filesystem::path(player_save_root);
  }

  return world_root_path(world_blocks_path, preset);
}

int32_t write_path_result(const std::filesystem::path &path, char *output,
                          uint64_t output_capacity, uint64_t *required_size) {
  if (required_size == nullptr) {
    return -1;
  }

  const std::string text = path.string();
  const uint64_t required = static_cast<uint64_t>(text.size()) + 1u;
  *required_size = required;
  if (output == nullptr || output_capacity == 0u) {
    return 0;
  }

  if (output_capacity < required) {
    return -2;
  }

  std::memcpy(output, text.c_str(), static_cast<std::size_t>(required));
  return 0;
}

int32_t write_root_child_path(const char *world_root, const char *filename,
                              char *path, uint64_t path_capacity,
                              uint64_t *required_size) {
  if (!has_value(world_root)) {
    return -1;
  }

  return write_path_result(std::filesystem::path(world_root) / filename, path,
                           path_capacity, required_size);
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_world_root_path_from_environment(
    char *path, uint64_t path_capacity, uint64_t *required_size) {
  return write_path_result(world_root_path(std::getenv(WorldBlocksPathEnv),
                                           std::getenv(BuildPresetEnv)),
                           path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_root_path_for_environment(
    const char *world_blocks_path, const char *build_preset, char *path,
    uint64_t path_capacity, uint64_t *required_size) {
  return write_path_result(world_root_path(world_blocks_path, build_preset),
                           path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_block_override_path_from_environment(
    char *path, uint64_t path_capacity, uint64_t *required_size) {
  return write_path_result(
      world_block_override_path(std::getenv(WorldBlocksPathEnv),
                                std::getenv(BuildPresetEnv)),
      path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_block_override_path_for_environment(
    const char *world_blocks_path, const char *build_preset, char *path,
    uint64_t path_capacity, uint64_t *required_size) {
  return write_path_result(
      world_block_override_path(world_blocks_path, build_preset), path,
      path_capacity, required_size);
}

int32_t octaryn_server_persistence_player_directory_path_from_environment(
    char *path, uint64_t path_capacity, uint64_t *required_size) {
  return write_path_result(
      player_directory_path(std::getenv(PlayerSaveRootEnv),
                            std::getenv(WorldBlocksPathEnv),
                            std::getenv(BuildPresetEnv)),
      path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_player_directory_path_for_environment(
    const char *player_save_root, const char *world_blocks_path,
    const char *build_preset, char *path, uint64_t path_capacity,
    uint64_t *required_size) {
  return write_path_result(
      player_directory_path(player_save_root, world_blocks_path, build_preset),
      path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_chunk_directory_for_aggregate_path(
    const char *aggregate_path, char *path, uint64_t path_capacity,
    uint64_t *required_size) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0') {
    return -1;
  }

  std::filesystem::path directory =
      std::filesystem::path(aggregate_path).parent_path();
  if (directory.empty()) {
    directory = ".";
  }

  return write_path_result(directory, path, path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_time_path_for_root(
    const char *world_root, char *path, uint64_t path_capacity,
    uint64_t *required_size) {
  return write_root_child_path(world_root, "world_time.json", path,
                               path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_block_override_path_for_root(
    const char *world_root, char *path, uint64_t path_capacity,
    uint64_t *required_size) {
  return write_root_child_path(world_root, "world_blocks.json", path,
                               path_capacity, required_size);
}

int32_t octaryn_server_persistence_world_metadata_path_for_root(
    const char *world_root, char *path, uint64_t path_capacity,
    uint64_t *required_size) {
  return write_root_child_path(world_root, "world_meta.json", path,
                               path_capacity, required_size);
}
}
