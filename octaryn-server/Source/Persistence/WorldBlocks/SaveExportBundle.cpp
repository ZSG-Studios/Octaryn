#include "WorldPersistence.h"

#include <glaze/glaze.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace octaryn::server::persistence {

struct save_export_world_time_file {
  uint32_t version = 1u;
  uint64_t day_index = 0u;
  double seconds_of_day = 0.0;
};

struct save_export_player_file {
  uint32_t version = 1u;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float pitch = 0.0F;
  float yaw = 0.0F;
  uint16_t block = 0u;
};

struct save_export_player_entry {
  int32_t id = 0;
  save_export_player_file data{};
};

struct save_export_chunk_block {
  int32_t bx = 0;
  int32_t by = 0;
  int32_t bz = 0;
  uint16_t block = 0u;
};

struct save_export_chunk_file {
  uint32_t version = 2u;
  int32_t cx = 0;
  int32_t cz = 0;
  std::vector<save_export_chunk_block> blocks{};
};

struct save_export_bundle_file {
  uint32_t version = 1u;
  std::optional<save_export_world_time_file> world_time{};
  std::vector<save_export_player_entry> players{};
  std::vector<save_export_chunk_file> chunks{};
};

} // namespace octaryn::server::persistence

namespace {

using octaryn::server::persistence::save_export_bundle_file;
using octaryn::server::persistence::save_export_chunk_block;
using octaryn::server::persistence::save_export_chunk_file;
using octaryn::server::persistence::save_export_player_entry;
using octaryn::server::persistence::save_export_player_file;
using octaryn::server::persistence::save_export_world_time_file;

constexpr uint32_t CurrentBundleVersion = 1u;
constexpr uint32_t CurrentPlayerVersion = 1u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts JsonWriteOptions{.prettify = true};

bool read_gzip_payload(const char *path, std::string &payload) {
  uint64_t payload_size = 0u;
  if (octaryn_server_persistence_read_gzip_file_count(path, &payload_size) !=
      0) {
    return false;
  }

  payload.assign(static_cast<size_t>(payload_size), '\0');
  uint64_t written = 0u;
  return octaryn_server_persistence_read_gzip_file_fill(
             path, reinterpret_cast<uint8_t *>(payload.data()), payload_size,
             &written) == 0 &&
         written == payload_size;
}

bool read_bundle(const char *path, save_export_bundle_file &bundle) {
  std::string payload;
  if (!read_gzip_payload(path, payload)) {
    return false;
  }

  if (glz::read<JsonReadOptions>(bundle, payload) ||
      bundle.version != CurrentBundleVersion) {
    return false;
  }

  for (const auto &player : bundle.players) {
    if (player.data.version != CurrentPlayerVersion) {
      return false;
    }
  }

  return true;
}

octaryn_server_persistence_save_export_bundle_counts
counts_for(const save_export_bundle_file &bundle) {
  uint32_t block_count = 0u;
  for (const auto &chunk : bundle.chunks) {
    block_count += static_cast<uint32_t>(chunk.blocks.size());
  }

  return octaryn_server_persistence_save_export_bundle_counts{
      .has_world_time = bundle.world_time.has_value() ? 1u : 0u,
      .player_count = static_cast<uint32_t>(bundle.players.size()),
      .chunk_count = static_cast<uint32_t>(bundle.chunks.size()),
      .block_count = block_count,
  };
}

save_export_bundle_file
bundle_from_abi(uint32_t bundle_version, uint32_t has_world_time,
                const octaryn_server_persistence_world_time_state *world_time,
                const octaryn_server_persistence_player_file_entry *players,
                uint32_t player_count,
                const octaryn_server_persistence_save_import_chunk *chunks,
                uint32_t chunk_count,
                const octaryn_server_persistence_chunk_override_block *blocks,
                uint32_t block_count) {
  save_export_bundle_file bundle{.version = bundle_version};
  if (has_world_time != 0u && world_time != nullptr) {
    bundle.world_time = save_export_world_time_file{
        .version = world_time->version,
        .day_index = world_time->day_index,
        .seconds_of_day = world_time->seconds_of_day,
    };
  }

  bundle.players.reserve(player_count);
  for (uint32_t index = 0u; index < player_count; ++index) {
    const auto &player = players[index];
    bundle.players.push_back(save_export_player_entry{
        .id = player.player_id,
        .data =
            save_export_player_file{
                .version = CurrentPlayerVersion,
                .x = player.state.x,
                .y = player.state.y,
                .z = player.state.z,
                .pitch = player.state.pitch,
                .yaw = player.state.yaw,
                .block = player.state.block,
            },
    });
  }

  bundle.chunks.reserve(chunk_count);
  for (uint32_t chunk_index = 0u; chunk_index < chunk_count; ++chunk_index) {
    const auto &chunk = chunks[chunk_index];
    save_export_chunk_file file{
        .version = chunk.version,
        .cx = chunk.cx,
        .cz = chunk.cz,
        .blocks = {},
    };

    const uint32_t end = chunk.block_offset + chunk.block_count;
    if (end <= block_count) {
      file.blocks.reserve(chunk.block_count);
      for (uint32_t block_index = chunk.block_offset; block_index < end;
           ++block_index) {
        const auto &block = blocks[block_index];
        file.blocks.push_back(save_export_chunk_block{
            .bx = block.bx,
            .by = block.by,
            .bz = block.bz,
            .block = block.block,
        });
      }
    }

    bundle.chunks.push_back(std::move(file));
  }

  return bundle;
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_save_export_bundle_count(
    const char *path,
    octaryn_server_persistence_save_export_bundle_counts *counts) {
  if (path == nullptr || path[0] == '\0' || counts == nullptr) {
    return -1;
  }

  save_export_bundle_file bundle{};
  if (!read_bundle(path, bundle)) {
    return -2;
  }

  *counts = counts_for(bundle);
  return 0;
}

int32_t octaryn_server_persistence_read_save_export_bundle_fill(
    const char *path, octaryn_server_persistence_world_time_state *world_time,
    octaryn_server_persistence_player_file_entry *players,
    uint32_t player_capacity,
    octaryn_server_persistence_save_import_chunk *chunks,
    uint32_t chunk_capacity,
    octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_capacity,
    octaryn_server_persistence_save_export_bundle_counts *written) {
  if (path == nullptr || path[0] == '\0' || written == nullptr) {
    return -1;
  }

  save_export_bundle_file bundle{};
  if (!read_bundle(path, bundle)) {
    return -2;
  }

  const auto counts = counts_for(bundle);
  *written = counts;
  if (player_capacity < counts.player_count ||
      chunk_capacity < counts.chunk_count ||
      block_capacity < counts.block_count) {
    return -3;
  }

  if ((counts.has_world_time != 0u && world_time == nullptr) ||
      (counts.player_count != 0u && players == nullptr) ||
      (counts.chunk_count != 0u && chunks == nullptr) ||
      (counts.block_count != 0u && blocks == nullptr)) {
    return -1;
  }

  if (bundle.world_time.has_value()) {
    *world_time = octaryn_server_persistence_world_time_state{
        .version = bundle.world_time->version,
        .day_index = bundle.world_time->day_index,
        .seconds_of_day = bundle.world_time->seconds_of_day,
    };
  }

  for (uint32_t index = 0u; index < bundle.players.size(); ++index) {
    const auto &player = bundle.players[index];
    players[index] = octaryn_server_persistence_player_file_entry{
        .player_id = player.id,
        .state =
            octaryn_server_persistence_player_state{
                .x = player.data.x,
                .y = player.data.y,
                .z = player.data.z,
                .pitch = player.data.pitch,
                .yaw = player.data.yaw,
                .block = player.data.block,
            },
    };
  }

  uint32_t block_offset = 0u;
  for (uint32_t chunk_index = 0u; chunk_index < bundle.chunks.size();
       ++chunk_index) {
    const auto &chunk = bundle.chunks[chunk_index];
    chunks[chunk_index] = octaryn_server_persistence_save_import_chunk{
        .version = chunk.version,
        .cx = chunk.cx,
        .cz = chunk.cz,
        .block_offset = block_offset,
        .block_count = static_cast<uint32_t>(chunk.blocks.size()),
    };

    for (const auto &block : chunk.blocks) {
      blocks[block_offset++] = octaryn_server_persistence_chunk_override_block{
          .bx = block.bx,
          .by = block.by,
          .bz = block.bz,
          .block = block.block,
      };
    }
  }

  return 0;
}

int32_t octaryn_server_persistence_write_save_export_bundle(
    const char *path, uint32_t bundle_version, uint32_t has_world_time,
    const octaryn_server_persistence_world_time_state *world_time,
    const octaryn_server_persistence_player_file_entry *players,
    uint32_t player_count,
    const octaryn_server_persistence_save_import_chunk *chunks,
    uint32_t chunk_count,
    const octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_count) {
  if (path == nullptr || path[0] == '\0' ||
      (player_count != 0u && players == nullptr) ||
      (chunk_count != 0u && chunks == nullptr) ||
      (block_count != 0u && blocks == nullptr) ||
      (has_world_time != 0u && world_time == nullptr)) {
    return -1;
  }

  for (uint32_t index = 0u; index < chunk_count; ++index) {
    const auto &chunk = chunks[index];
    if (chunk.block_offset > block_count ||
        block_count - chunk.block_offset < chunk.block_count) {
      return -3;
    }
  }

  const save_export_bundle_file bundle =
      bundle_from_abi(bundle_version, has_world_time, world_time, players,
                      player_count, chunks, chunk_count, blocks, block_count);
  std::string payload;
  if (glz::write<JsonWriteOptions>(bundle, payload)) {
    return -2;
  }

  return octaryn_server_persistence_write_gzip_file(
      path, reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
}
}
