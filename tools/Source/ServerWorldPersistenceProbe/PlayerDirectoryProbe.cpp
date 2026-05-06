#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

bool validate_player_directory_scan() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_player_directory_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  const octaryn_server_persistence_player_state player_two{
      .x = 2.0f,
      .y = 64.0f,
      .z = 4.0f,
      .pitch = 10.0f,
      .yaw = 20.0f,
      .block = 7u,
  };
  const octaryn_server_persistence_player_state player_one{
      .x = 1.0f,
      .y = 70.0f,
      .z = 3.0f,
      .pitch = -5.0f,
      .yaw = 90.0f,
      .block = 11u,
  };

  bool ok = true;
  ok &= expect_equal("write player 2",
                     octaryn_server_persistence_write_player_file(
                         (root / "player_2.json").string().c_str(),
                         &player_two),
                     0);
  ok &= expect_equal("write player 1",
                     octaryn_server_persistence_write_player_file(
                         (root / "player_1.json").string().c_str(),
                         &player_one),
                     0);
  ok &= expect_equal("write player 4 directory entry",
                     octaryn_server_persistence_write_player_directory_entry(
                         root.string().c_str(), 4, &player_one),
                     0);
  const std::string player_four_path = (root / "player_4.json").string();
  uint64_t required_path_size = 0u;
  ok &= expect_equal("player directory path count",
                     octaryn_server_persistence_player_directory_path(
                         root.string().c_str(), 4, nullptr, 0,
                         &required_path_size),
                     0);
  if (required_path_size == 0u) {
    return false;
  }
  std::vector<char> path_buffer(required_path_size);
  uint64_t written_path_size = 0u;
  ok &= expect_equal("player directory path fill",
                     octaryn_server_persistence_player_directory_path(
                         root.string().c_str(), 4, path_buffer.data(),
                         static_cast<uint64_t>(path_buffer.size()),
                         &written_path_size),
                     0);
  ok &= expect_equal("player directory path size", written_path_size,
                     required_path_size);
  ok &= expect_equal("player directory path", std::string(path_buffer.data()),
                     player_four_path);

  octaryn_server_persistence_player_state loaded_player_four{};
  ok &= expect_equal("read player 4 directory entry",
                     octaryn_server_persistence_read_player_directory_entry(
                         root.string().c_str(), 4, &loaded_player_four),
                     0);
  ok &= expect_equal("read player 4 block", loaded_player_four.block,
                     player_one.block);
  ok &= expect_equal("missing player directory entry",
                     octaryn_server_persistence_read_player_directory_entry(
                         root.string().c_str(), 99, &loaded_player_four),
                     1);

  {
    std::ofstream unsupported(root / "player_3.json",
                              std::ios::binary | std::ios::trunc);
    unsupported
        << R"({"version":99,"x":0,"y":0,"z":0,"pitch":0,"yaw":0,"block":1})";
  }
  {
    std::ofstream ignored(root / "not_a_player.json",
                          std::ios::binary | std::ios::trunc);
    ignored << "{}";
  }

  uint32_t player_count = 0u;
  ok &= expect_equal("player directory count",
                     octaryn_server_persistence_read_player_directory_count(
                         root.string().c_str(), &player_count),
                     0);
  ok &= expect_equal("valid player count", player_count, 3u);

  std::vector<octaryn_server_persistence_player_file_entry> players(
      player_count);
  uint32_t written = 0u;
  ok &= expect_equal("player directory fill",
                     octaryn_server_persistence_read_player_directory_fill(
                         root.string().c_str(), players.data(), player_count,
                         &written),
                     0);
  ok &= expect_equal("written player count", written, 3u);
  ok &= expect_equal("first player id", players[0].player_id, 1);
  ok &= expect_equal("first player block", players[0].state.block,
                     player_one.block);
  ok &= expect_equal("second player id", players[1].player_id, 2);
  ok &= expect_equal("second player x", players[1].state.x, player_two.x);
  ok &= expect_equal("third player id", players[2].player_id, 4);
  ok &= expect_equal("third player block", players[2].state.block,
                     player_one.block);

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
