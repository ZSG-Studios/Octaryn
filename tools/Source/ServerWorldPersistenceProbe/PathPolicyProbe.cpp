#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {
namespace {

void set_environment_value(const char *name, const char *value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void clear_environment_value(const char *name) {
#if defined(_WIN32)
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

std::string read_path(int32_t (*read)(char *, uint64_t, uint64_t *)) {
  uint64_t required = 0;
  if (read(nullptr, 0, &required) != 0 || required == 0u) {
    return {};
  }

  std::vector<char> buffer(static_cast<std::size_t>(required));
  uint64_t written = 0;
  if (read(buffer.data(), required, &written) != 0 || written != required) {
    return {};
  }

  return std::string(buffer.data());
}

std::string read_chunk_directory(const char *aggregate_path) {
  uint64_t required = 0;
  if (octaryn_server_persistence_chunk_directory_for_aggregate_path(
          aggregate_path, nullptr, 0, &required) != 0 ||
      required == 0u) {
    return {};
  }

  std::vector<char> buffer(static_cast<std::size_t>(required));
  uint64_t written = 0;
  if (octaryn_server_persistence_chunk_directory_for_aggregate_path(
          aggregate_path, buffer.data(), required, &written) != 0 ||
      written != required) {
    return {};
  }

  return std::string(buffer.data());
}

std::string read_root_child_path(int32_t (*read)(const char *, char *, uint64_t,
                                                 uint64_t *),
                                 const char *world_root) {
  uint64_t required = 0;
  if (read(world_root, nullptr, 0, &required) != 0 || required == 0u) {
    return {};
  }

  std::vector<char> buffer(static_cast<std::size_t>(required));
  uint64_t written = 0;
  if (read(world_root, buffer.data(), required, &written) != 0 ||
      written != required) {
    return {};
  }

  return std::string(buffer.data());
}

void clear_path_environment() {
  clear_environment_value("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
  clear_environment_value("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
  clear_environment_value("OctarynBuildPresetName");
}

} // namespace

bool validate_path_policy() {
  clear_path_environment();

  bool ok = true;
  ok &= expect_equal(
      "default world root",
      read_path(octaryn_server_persistence_world_root_path_from_environment),
      std::filesystem::path("build/debug-linux/server/world").string());
  ok &= expect_equal(
      "default world blocks path",
      read_path(
          octaryn_server_persistence_world_block_override_path_from_environment),
      std::filesystem::path("build/debug-linux/server/world/world_blocks.json")
          .string());
  ok &= expect_equal(
      "default player directory",
      read_path(
          octaryn_server_persistence_player_directory_path_from_environment),
      std::filesystem::path("build/debug-linux/server/world").string());

  set_environment_value("OctarynBuildPresetName", "release-linux");
  ok &= expect_equal(
      "preset world blocks path",
      read_path(
          octaryn_server_persistence_world_block_override_path_from_environment),
      std::filesystem::path(
          "build/release-linux/server/world/world_blocks.json")
          .string());

  set_environment_value("OCTARYN_SERVER_WORLD_BLOCKS_PATH",
                        "/tmp/octaryn/custom/world_blocks.json");
  ok &= expect_equal(
      "explicit world root",
      read_path(octaryn_server_persistence_world_root_path_from_environment),
      std::filesystem::path("/tmp/octaryn/custom").string());
  ok &= expect_equal(
      "explicit world blocks path",
      read_path(
          octaryn_server_persistence_world_block_override_path_from_environment),
      std::filesystem::path("/tmp/octaryn/custom/world_blocks.json").string());
  ok &= expect_equal(
      "player directory from world blocks path",
      read_path(
          octaryn_server_persistence_player_directory_path_from_environment),
      std::filesystem::path("/tmp/octaryn/custom").string());

  set_environment_value("OCTARYN_SERVER_PLAYER_SAVE_ROOT",
                        "/tmp/octaryn/players");
  ok &= expect_equal(
      "explicit player directory",
      read_path(
          octaryn_server_persistence_player_directory_path_from_environment),
      std::filesystem::path("/tmp/octaryn/players").string());

  ok &= expect_equal(
      "chunk directory for aggregate path",
      read_chunk_directory("/tmp/octaryn/custom/world_blocks.json"),
      std::filesystem::path("/tmp/octaryn/custom").string());
  ok &= expect_equal("chunk directory for local aggregate",
                     read_chunk_directory("world_blocks.json"),
                     std::filesystem::path(".").string());
  ok &= expect_equal(
      "world time path for root",
      read_root_child_path(octaryn_server_persistence_world_time_path_for_root,
                           "/tmp/octaryn/custom"),
      std::filesystem::path("/tmp/octaryn/custom/world_time.json").string());
  ok &= expect_equal(
      "world block path for root",
      read_root_child_path(
          octaryn_server_persistence_world_block_override_path_for_root,
          "/tmp/octaryn/custom"),
      std::filesystem::path("/tmp/octaryn/custom/world_blocks.json").string());
  ok &= expect_equal(
      "world metadata path for root",
      read_root_child_path(
          octaryn_server_persistence_world_metadata_path_for_root,
          "/tmp/octaryn/custom"),
      std::filesystem::path("/tmp/octaryn/custom/world_meta.json").string());

  uint64_t required = 0;
  ok &= expect_equal(
      "missing aggregate path",
      octaryn_server_persistence_chunk_directory_for_aggregate_path(
          nullptr, nullptr, 0, &required),
      -1);

  clear_path_environment();
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
