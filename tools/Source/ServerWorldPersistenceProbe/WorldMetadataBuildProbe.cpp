#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

namespace {

octaryn_server_persistence_block_edit metadata_edit(int32_t x, int32_t y,
                                                   int32_t z, uint16_t block) {
  return octaryn_server_persistence_block_edit{
      .position = {.x = x, .y = y, .z = z},
      .block = block,
  };
}

} // namespace

bool validate_world_metadata_build() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_metadata_build_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  octaryn_server_persistence_world_metadata metadata{};
  bool ok = true;
  ok &= expect_equal("empty metadata build",
                     octaryn_server_persistence_build_world_metadata(
                         root.string().c_str(), &metadata),
                     0);
  ok &= expect_equal("empty metadata save", metadata.save_exists, 0u);
  ok &= expect_equal("empty metadata players", metadata.player_count, 0);
  ok &= expect_equal("empty metadata chunks", metadata.chunk_override_count, 0);

  const octaryn_server_persistence_world_time_state world_time{
      .version = 1u,
      .day_index = 3u,
      .seconds_of_day = 42.0,
  };
  ok &= expect_equal("metadata world time write",
                     octaryn_server_persistence_write_world_time_file(
                         (root / "world_time.json").string().c_str(),
                         &world_time),
                     0);

  const octaryn_server_persistence_player_state player{
      .x = 1.0f,
      .y = 2.0f,
      .z = 3.0f,
      .pitch = 4.0f,
      .yaw = 5.0f,
      .block = 6u,
  };
  ok &= expect_equal("metadata player one write",
                     octaryn_server_persistence_write_player_directory_entry(
                         root.string().c_str(), 1, &player),
                     0);
  ok &= expect_equal("metadata player two write",
                     octaryn_server_persistence_write_player_directory_entry(
                         root.string().c_str(), 2, &player),
                     0);

  const std::vector<octaryn_server_persistence_block_edit> edits{
      metadata_edit(0, 1, 0, 7),
      metadata_edit(33, 1, 0, 8),
      metadata_edit(0, 1, 33, 9),
  };
  ok &= expect_equal("metadata world blocks save",
                     octaryn_server_persistence_save_world_block_overrides(
                         (root / "world_blocks.json").string().c_str(),
                         root.string().c_str(), edits.data(),
                         static_cast<uint32_t>(edits.size())),
                     0);

  ok &= expect_equal("filled metadata build",
                     octaryn_server_persistence_build_world_metadata(
                         root.string().c_str(), &metadata),
                     0);
  ok &= expect_equal("filled metadata save", metadata.save_exists, 1u);
  ok &= expect_equal("filled metadata time", metadata.has_world_time, 1u);
  ok &= expect_equal("filled metadata player flag", metadata.has_player_data,
                     1u);
  ok &= expect_equal("filled metadata world flag", metadata.has_world_data, 1u);
  ok &= expect_equal("filled metadata players", metadata.player_count, 2);
  ok &= expect_equal("filled metadata chunks", metadata.chunk_override_count, 3);

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
