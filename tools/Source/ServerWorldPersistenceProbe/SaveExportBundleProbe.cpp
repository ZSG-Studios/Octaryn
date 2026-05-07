#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

bool validate_save_export_bundle_codec() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_save_export_bundle_probe.json.gz";
  const std::filesystem::path unsupported_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_save_export_bundle_probe_unsupported.json.gz";
  std::error_code error;
  std::filesystem::remove(path, error);
  std::filesystem::remove(unsupported_path, error);

  const octaryn_server_persistence_world_time_state world_time{
      .version = 1u,
      .day_index = 8u,
      .seconds_of_day = 42.25,
  };
  const std::vector<octaryn_server_persistence_player_file_entry> players{
      {.player_id = 1,
       .state = {.x = -10.5F,
                 .y = 64.0F,
                 .z = 5.25F,
                 .pitch = 12.0F,
                 .yaw = 90.0F,
                 .block = 7u}},
      {.player_id = 2,
       .state = {.x = 16.0F,
                 .y = 70.0F,
                 .z = -3.0F,
                 .pitch = -2.0F,
                 .yaw = 180.0F,
                 .block = 11u}},
  };
  const std::vector<octaryn_server_persistence_save_import_chunk> chunks{
      {.version = 2u,
       .cx = -32,
       .cz = 0,
       .block_offset = 0u,
       .block_count = 1u},
      {.version = 1u, .cx = 32, .cz = 0, .block_offset = 1u, .block_count = 1u},
  };
  const std::vector<octaryn_server_persistence_chunk_override_block> blocks{
      {.bx = -1, .by = 2, .bz = 31, .block = 6u},
      {.bx = 1, .by = 3, .bz = 0, .block = 7u},
  };

  bool ok = true;
  ok &= expect_equal("save export write",
                     octaryn_server_persistence_write_save_export_bundle(
                         path.string().c_str(), 1u, 1u, &world_time,
                         players.data(), static_cast<uint32_t>(players.size()),
                         chunks.data(), static_cast<uint32_t>(chunks.size()),
                         blocks.data(), static_cast<uint32_t>(blocks.size())),
                     0);

  octaryn_server_persistence_save_export_bundle_counts counts{};
  ok &= expect_equal("save export count",
                     octaryn_server_persistence_read_save_export_bundle_count(
                         path.string().c_str(), &counts),
                     0);
  ok &= expect_equal("save export has world time", counts.has_world_time, 1u);
  ok &= expect_equal("save export player count", counts.player_count, 2u);
  ok &= expect_equal("save export chunk count", counts.chunk_count, 2u);
  ok &= expect_equal("save export block count", counts.block_count, 2u);

  octaryn_server_persistence_world_time_state loaded_time{};
  std::vector<octaryn_server_persistence_player_file_entry> loaded_players(
      counts.player_count);
  std::vector<octaryn_server_persistence_save_import_chunk> loaded_chunks(
      counts.chunk_count);
  std::vector<octaryn_server_persistence_chunk_override_block> loaded_blocks(
      counts.block_count);
  octaryn_server_persistence_save_export_bundle_counts written{};
  ok &= expect_equal(
      "save export fill",
      octaryn_server_persistence_read_save_export_bundle_fill(
          path.string().c_str(), &loaded_time, loaded_players.data(),
          static_cast<uint32_t>(loaded_players.size()), loaded_chunks.data(),
          static_cast<uint32_t>(loaded_chunks.size()), loaded_blocks.data(),
          static_cast<uint32_t>(loaded_blocks.size()), &written),
      0);
  ok &= expect_equal("save export loaded day", loaded_time.day_index, 8u);
  ok &=
      expect_equal("save export loaded player", loaded_players[1].player_id, 2);
  ok &= expect_equal("save export preserves legacy chunk version",
                     loaded_chunks[1].version, 1u);
  ok &= expect_equal("save export loaded second block", loaded_blocks[1].block,
                     uint16_t{7});

  ok &= expect_equal("save export unsupported write",
                     octaryn_server_persistence_write_save_export_bundle(
                         unsupported_path.string().c_str(), 99u, 0u, nullptr,
                         nullptr, 0u, nullptr, 0u, nullptr, 0u),
                     0);
  ok &= expect_equal("save export rejects unsupported bundle",
                     octaryn_server_persistence_read_save_export_bundle_count(
                         unsupported_path.string().c_str(), &counts) != 0,
                     true);

  std::filesystem::remove(path, error);
  std::filesystem::remove(unsupported_path, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
