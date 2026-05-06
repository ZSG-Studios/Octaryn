#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <filesystem>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

bool validate_world_save_import_bundle() {
  const std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     "octaryn_server_world_save_import_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  const octaryn_server_persistence_world_time_state world_time{
      .version = 1u,
      .day_index = 8u,
      .seconds_of_day = 42.25,
  };
  const std::vector<octaryn_server_persistence_player_file_entry> players{
      {
          .player_id = 7,
          .state = {.x = 1.0f,
                    .y = 2.0f,
                    .z = 3.0f,
                    .pitch = 4.0f,
                    .yaw = 5.0f,
                    .block = 6u},
      },
  };
  const std::vector<octaryn_server_persistence_save_import_chunk> chunks{
      {
          .version = 1u,
          .cx = 64,
          .cz = 0,
          .block_offset = 0u,
          .block_count = 1u,
      },
      {
          .version = 2u,
          .cx = -32,
          .cz = 0,
          .block_offset = 1u,
          .block_count = 1u,
      },
  };
  const std::vector<octaryn_server_persistence_chunk_override_block> blocks{
      {.bx = 1, .by = 2, .bz = 3, .block = 10u},
      {.bx = -1, .by = 4, .bz = 31, .block = 11u},
  };

  bool ok = true;
  ok &= expect_equal("import save export bundle",
                     octaryn_server_persistence_import_save_export_bundle(
                         root.string().c_str(), 1u, &world_time, players.data(),
                         static_cast<uint32_t>(players.size()), chunks.data(),
                         static_cast<uint32_t>(chunks.size()), blocks.data(),
                         static_cast<uint32_t>(blocks.size())),
                     0);

  octaryn_server_persistence_world_time_state loaded_time{};
  ok &= expect_equal(
      "import world time",
      octaryn_server_persistence_read_world_time_file(
          (root / "world_time.json").string().c_str(), &loaded_time),
      0);
  ok &= expect_equal("import world time day", loaded_time.day_index, 8u);
  ok &= expect_equal("import world time seconds", loaded_time.seconds_of_day,
                     42.25);

  octaryn_server_persistence_player_state loaded_player{};
  ok &= expect_equal("import player",
                     octaryn_server_persistence_read_player_directory_entry(
                         root.string().c_str(), 7, &loaded_player),
                     0);
  ok &= expect_equal("import player block", loaded_player.block, uint16_t{6});

  uint32_t block_count = 0u;
  ok &=
      expect_equal("imported world block count",
                   octaryn_server_persistence_read_world_block_overrides_count(
                       (root / "world_blocks.json").string().c_str(),
                       root.string().c_str(), &block_count),
                   0);
  ok &= expect_equal("imported world block total", block_count, 2u);

  std::vector<octaryn_server_persistence_block_edit> imported(block_count);
  uint32_t written = 0u;
  ok &= expect_equal("imported world block fill",
                     octaryn_server_persistence_read_world_block_overrides_fill(
                         (root / "world_blocks.json").string().c_str(),
                         root.string().c_str(), imported.data(), block_count,
                         &written),
                     0);
  ok &= expect_equal("imported world block written", written, 2u);
  ok &= expect_equal("legacy import normalized x", imported[1].position.x, 65);
  ok &= expect_equal("legacy import normalized z", imported[1].position.z, 3);

  uint32_t column_count = 0u;
  ok &= expect_equal(
      "imported chunk column count",
      octaryn_server_persistence_count_world_block_override_columns(
          (root / "world_blocks.json").string().c_str(), &column_count),
      0);
  ok &= expect_equal("imported chunk columns", column_count, 2u);

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
