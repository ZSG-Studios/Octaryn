#include "WorldPersistence.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr uint32_t WorldTimeVersion = 1u;

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

int32_t import_world_time(
    const char *world_root, uint32_t has_world_time,
    const octaryn_server_persistence_world_time_state *world_time) {
  if (has_world_time == 0u) {
    return 0;
  }
  if (world_time == nullptr || world_time->version != WorldTimeVersion) {
    return -1;
  }

  const std::filesystem::path path = root_child_path(
      world_root, octaryn_server_persistence_world_time_path_for_root);
  if (path.empty()) {
    return -1;
  }

  return octaryn_server_persistence_write_world_time_file(path.string().c_str(),
                                                          world_time);
}

int32_t
import_players(const char *world_root,
               const octaryn_server_persistence_player_file_entry *players,
               uint32_t player_count) {
  for (uint32_t index = 0; index < player_count; ++index) {
    const int32_t result =
        octaryn_server_persistence_write_player_directory_entry(
            world_root, players[index].player_id, &players[index].state);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

int32_t append_normalized_chunk_edits(
    const octaryn_server_persistence_save_import_chunk &chunk,
    const octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_count,
    std::vector<octaryn_server_persistence_block_edit> &edits) {
  if (chunk.block_offset > block_count ||
      chunk.block_count > block_count - chunk.block_offset) {
    return -2;
  }

  const octaryn_server_persistence_chunk_override_file file{
      .version = chunk.version,
      .cx = chunk.cx,
      .cz = chunk.cz,
      .block_count = chunk.block_count,
  };
  const octaryn_server_persistence_chunk_override_block *source =
      chunk.block_count == 0u ? nullptr : blocks + chunk.block_offset;
  std::vector<octaryn_server_persistence_chunk_override_block> normalized(
      chunk.block_count);
  octaryn_server_persistence_chunk_override_file normalized_file{};
  const int32_t result =
      octaryn_server_persistence_normalize_chunk_override_file(
          &file, source, normalized.data(),
          static_cast<uint32_t>(normalized.size()), &normalized_file);
  if (result != 0 || normalized_file.block_count != chunk.block_count) {
    return result != 0 ? result : -3;
  }

  for (const auto &block : normalized) {
    edits.push_back(octaryn_server_persistence_block_edit{
        .position = {.x = block.bx, .y = block.by, .z = block.bz},
        .block = block.block,
    });
  }
  return 0;
}

int32_t
import_chunks(const char *world_root,
              const octaryn_server_persistence_save_import_chunk *chunks,
              uint32_t chunk_count,
              const octaryn_server_persistence_chunk_override_block *blocks,
              uint32_t block_count) {
  std::vector<octaryn_server_persistence_block_edit> edits;
  edits.reserve(block_count);
  for (uint32_t index = 0; index < chunk_count; ++index) {
    const int32_t result = append_normalized_chunk_edits(chunks[index], blocks,
                                                         block_count, edits);
    if (result != 0) {
      return result;
    }
  }

  const std::filesystem::path aggregate_path = root_child_path(
      world_root,
      octaryn_server_persistence_world_block_override_path_for_root);
  if (aggregate_path.empty()) {
    return -1;
  }

  return octaryn_server_persistence_save_world_block_overrides(
      aggregate_path.string().c_str(), world_root, edits.data(),
      static_cast<uint32_t>(edits.size()));
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_import_save_export_bundle(
    const char *world_root, uint32_t has_world_time,
    const octaryn_server_persistence_world_time_state *world_time,
    const octaryn_server_persistence_player_file_entry *players,
    uint32_t player_count,
    const octaryn_server_persistence_save_import_chunk *chunks,
    uint32_t chunk_count,
    const octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_count) {
  if (world_root == nullptr || world_root[0] == '\0' ||
      (players == nullptr && player_count != 0u) ||
      (chunks == nullptr && chunk_count != 0u) ||
      (blocks == nullptr && block_count != 0u)) {
    return -1;
  }

  int32_t result = import_world_time(world_root, has_world_time, world_time);
  if (result != 0) {
    return result;
  }

  result = import_players(world_root, players, player_count);
  if (result != 0) {
    return result;
  }

  return import_chunks(world_root, chunks, chunk_count, blocks, block_count);
}
}
