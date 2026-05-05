#include "WorldPersistence.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

octaryn_server_persistence_block_edit edit(int32_t x, int32_t y, int32_t z,
                                           uint16_t block) {
  return octaryn_server_persistence_block_edit{
      .position = {.x = x, .y = y, .z = z},
      .block = block,
  };
}

bool validate_chunk_column_plan() {
  const std::vector<octaryn_server_persistence_block_edit> edits{
      edit(33, 4, 0, 7),
      edit(0, 3, 0, 6),
      edit(-1, 2, -1, 5),
  };

  octaryn_server_persistence_plan_counts counts{};
  bool ok = true;
  ok &= expect_equal("count result",
                     octaryn_server_persistence_plan_chunk_columns_count(
                         edits.data(), static_cast<uint32_t>(edits.size()),
                         &counts),
                     0);
  ok &= expect_equal("column count", counts.column_count, 3u);
  ok &= expect_equal("block count", counts.block_count, 3u);

  std::vector<octaryn_server_persistence_chunk_column> columns(
      counts.column_count);
  std::vector<octaryn_server_persistence_block_edit> ordered(
      counts.block_count);
  octaryn_server_persistence_plan_counts written{};
  ok &= expect_equal(
      "fill result",
      octaryn_server_persistence_plan_chunk_columns_fill(
          edits.data(), static_cast<uint32_t>(edits.size()), columns.data(),
          static_cast<uint32_t>(columns.size()), ordered.data(),
          static_cast<uint32_t>(ordered.size()), &written),
      0);
  ok &= expect_equal("first origin x", columns[0].origin_x, -32);
  ok &= expect_equal("first origin z", columns[0].origin_z, -32);
  ok &= expect_equal("second origin x", columns[1].origin_x, 0);
  ok &= expect_equal("third origin x", columns[2].origin_x, 32);
  ok &= expect_equal("ordered first block", ordered[0].block, uint16_t{5});
  ok &= expect_equal("ordered second block", ordered[1].block, uint16_t{6});
  ok &= expect_equal("ordered third block", ordered[2].block, uint16_t{7});
  return ok;
}

bool validate_gzip_round_trip() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_persistence_probe.gzip";
  const std::string payload = "octaryn native gzip persistence";

  bool ok = true;
  ok &= expect_equal(
      "gzip write",
      octaryn_server_persistence_write_gzip_file(
          path.string().c_str(),
          reinterpret_cast<const uint8_t *>(payload.data()), payload.size()),
      0);

  uint64_t payload_size = 0u;
  ok &= expect_equal("gzip count",
                     octaryn_server_persistence_read_gzip_file_count(
                         path.string().c_str(), &payload_size),
                     0);
  ok &= expect_equal("gzip payload size", payload_size,
                     static_cast<uint64_t>(payload.size()));

  std::vector<uint8_t> read_payload(payload_size);
  uint64_t written = 0u;
  ok &= expect_equal("gzip fill",
                     octaryn_server_persistence_read_gzip_file_fill(
                         path.string().c_str(), read_payload.data(),
                         read_payload.size(), &written),
                     0);
  ok &= expect_equal("gzip written size", written,
                     static_cast<uint64_t>(payload.size()));
  ok &= expect_equal("gzip payload",
                     std::string_view(
                         reinterpret_cast<const char *>(read_payload.data()),
                         read_payload.size()),
                     std::string_view(payload));

  std::error_code error;
  std::filesystem::remove(path, error);
  return ok;
}

bool validate_player_file_round_trip() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_player_persistence_probe.json";
  std::error_code error;
  std::filesystem::remove(path, error);

  octaryn_server_persistence_player_state loaded{};
  bool ok = true;
  ok &= expect_equal("missing player file",
                     octaryn_server_persistence_read_player_file(
                         path.string().c_str(), &loaded),
                     1);

  const octaryn_server_persistence_player_state state{
      .x = -10.5f,
      .y = 64.0f,
      .z = 5.25f,
      .pitch = 12.0f,
      .yaw = 90.0f,
      .block = 7u,
  };
  ok &= expect_equal("player write",
                     octaryn_server_persistence_write_player_file(
                         path.string().c_str(), &state),
                     0);
  ok &= expect_equal("player read",
                     octaryn_server_persistence_read_player_file(
                         path.string().c_str(), &loaded),
                     0);
  ok &= expect_equal("player x", loaded.x, state.x);
  ok &= expect_equal("player y", loaded.y, state.y);
  ok &= expect_equal("player z", loaded.z, state.z);
  ok &= expect_equal("player pitch", loaded.pitch, state.pitch);
  ok &= expect_equal("player yaw", loaded.yaw, state.yaw);
  ok &= expect_equal("player block", loaded.block, state.block);

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << R"({"version":99,"x":0,"y":0,"z":0,"pitch":0,"yaw":0,"block":1})";
  }
  ok &= expect_equal("unsupported player version",
                     octaryn_server_persistence_read_player_file(
                         path.string().c_str(), &loaded),
                     -3);

  std::filesystem::remove(path, error);
  return ok;
}

bool validate_chunk_override_file_round_trip() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_chunk_override_persistence_probe.json";
  std::error_code error;
  std::filesystem::remove(path, error);

  octaryn_server_persistence_chunk_override_file loaded_file{};
  bool ok = true;
  ok &= expect_equal("missing chunk override file",
                     octaryn_server_persistence_read_chunk_override_file_count(
                         path.string().c_str(), &loaded_file),
                     1);

  const std::vector<octaryn_server_persistence_chunk_override_block> blocks{
      {.bx = -1, .by = 2, .bz = -1, .block = 5u},
      {.bx = 0, .by = 3, .bz = 0, .block = 6u},
  };
  const octaryn_server_persistence_chunk_override_file file{
      .version = 2u,
      .cx = -32,
      .cz = -32,
      .block_count = static_cast<uint32_t>(blocks.size()),
  };
  ok &= expect_equal("chunk override write",
                     octaryn_server_persistence_write_chunk_override_file(
                         path.string().c_str(), &file, blocks.data()),
                     0);
  ok &= expect_equal("chunk override count",
                     octaryn_server_persistence_read_chunk_override_file_count(
                         path.string().c_str(), &loaded_file),
                     0);
  ok &= expect_equal("chunk override version", loaded_file.version, 2u);
  ok &= expect_equal("chunk override cx", loaded_file.cx, -32);
  ok &= expect_equal("chunk override block count", loaded_file.block_count,
                     static_cast<uint32_t>(blocks.size()));

  std::vector<octaryn_server_persistence_chunk_override_block> loaded_blocks(
      loaded_file.block_count);
  ok &= expect_equal("chunk override fill",
                     octaryn_server_persistence_read_chunk_override_file_fill(
                         path.string().c_str(), loaded_blocks.data(),
                         static_cast<uint32_t>(loaded_blocks.size()),
                         &loaded_file),
                     0);
  ok &= expect_equal("chunk override first x", loaded_blocks[0].bx, -1);
  ok &= expect_equal("chunk override second block", loaded_blocks[1].block,
                     uint16_t{6});

  {
    std::ofstream legacy(path, std::ios::binary | std::ios::trunc);
    legacy << R"({"version":1,"cx":32,"cz":64,"blocks":[{"bx":0,"by":5,"bz":1,"block":9}]})";
  }
  ok &= expect_equal("legacy chunk override count",
                     octaryn_server_persistence_read_chunk_override_file_count(
                         path.string().c_str(), &loaded_file),
                     0);
  loaded_blocks.assign(loaded_file.block_count, {});
  ok &= expect_equal("legacy chunk override fill",
                     octaryn_server_persistence_read_chunk_override_file_fill(
                         path.string().c_str(), loaded_blocks.data(),
                         static_cast<uint32_t>(loaded_blocks.size()),
                         &loaded_file),
                     0);
  ok &= expect_equal("legacy chunk override upgraded version",
                     loaded_file.version, 2u);
  ok &= expect_equal("legacy chunk override world x", loaded_blocks[0].bx, 32);
  ok &= expect_equal("legacy chunk override world z", loaded_blocks[0].bz, 65);

  {
    std::ofstream unsupported(path, std::ios::binary | std::ios::trunc);
    unsupported << R"({"version":99,"cx":0,"cz":0,"blocks":[]})";
  }
  ok &= expect_equal("unsupported chunk override version",
                     octaryn_server_persistence_read_chunk_override_file_count(
                         path.string().c_str(), &loaded_file),
                     -2);

  std::filesystem::remove(path, error);
  return ok;
}

bool validate_world_time_file_round_trip() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_time_persistence_probe.json";
  std::error_code error;
  std::filesystem::remove(path, error);

  octaryn_server_persistence_world_time_state loaded{};
  bool ok = true;
  ok &= expect_equal("missing world time file",
                     octaryn_server_persistence_read_world_time_file(
                         path.string().c_str(), &loaded),
                     1);

  const octaryn_server_persistence_world_time_state state{
      .version = 1u,
      .day_index = 8u,
      .seconds_of_day = 42.25,
  };
  ok &= expect_equal("world time write",
                     octaryn_server_persistence_write_world_time_file(
                         path.string().c_str(), &state),
                     0);
  ok &= expect_equal("world time read",
                     octaryn_server_persistence_read_world_time_file(
                         path.string().c_str(), &loaded),
                     0);
  ok &= expect_equal("world time version", loaded.version, state.version);
  ok &= expect_equal("world time day", loaded.day_index, state.day_index);
  ok &= expect_equal("world time seconds", loaded.seconds_of_day,
                     state.seconds_of_day);

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << R"({"version":99,"day_index":0,"seconds_of_day":0})";
  }
  ok &= expect_equal("unsupported world time version",
                     octaryn_server_persistence_read_world_time_file(
                         path.string().c_str(), &loaded),
                     -3);

  std::filesystem::remove(path, error);
  return ok;
}

bool validate_world_metadata_file_round_trip() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_metadata_persistence_probe.json";
  std::error_code error;
  std::filesystem::remove(path, error);

  octaryn_server_persistence_world_metadata loaded{};
  bool ok = true;
  ok &= expect_equal("missing world metadata file",
                     octaryn_server_persistence_read_world_metadata_file(
                         path.string().c_str(), &loaded),
                     1);

  const octaryn_server_persistence_world_metadata metadata{
      .save_exists = 1u,
      .has_world_time = 1u,
      .has_player_data = 1u,
      .has_world_data = 1u,
      .player_count = 2,
      .chunk_override_count = 3,
  };
  ok &= expect_equal("world metadata write",
                     octaryn_server_persistence_write_world_metadata_file(
                         path.string().c_str(), &metadata),
                     0);
  ok &= expect_equal("world metadata read",
                     octaryn_server_persistence_read_world_metadata_file(
                         path.string().c_str(), &loaded),
                     0);
  ok &= expect_equal("world metadata save_exists", loaded.save_exists,
                     metadata.save_exists);
  ok &= expect_equal("world metadata time", loaded.has_world_time,
                     metadata.has_world_time);
  ok &= expect_equal("world metadata player count", loaded.player_count,
                     metadata.player_count);
  ok &= expect_equal("world metadata chunk count", loaded.chunk_override_count,
                     metadata.chunk_override_count);

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << R"({"version":99,"save_exists":true})";
  }
  ok &= expect_equal("unsupported world metadata version",
                     octaryn_server_persistence_read_world_metadata_file(
                         path.string().c_str(), &loaded),
                     -3);

  std::filesystem::remove(path, error);
  return ok;
}

} // namespace

int main() {
  if (!validate_chunk_column_plan()) {
    return 1;
  }
  if (!validate_gzip_round_trip()) {
    return 1;
  }
  if (!validate_chunk_override_file_round_trip()) {
    return 1;
  }
  if (!validate_player_file_round_trip()) {
    return 1;
  }
  if (!validate_world_time_file_round_trip()) {
    return 1;
  }
  if (!validate_world_metadata_file_round_trip()) {
    return 1;
  }

  std::puts("server world persistence native probe passed");
  return 0;
}
