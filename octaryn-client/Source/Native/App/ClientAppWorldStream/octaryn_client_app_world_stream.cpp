#include "octaryn_client_app_world_stream.h"

#include "octaryn_client_app_file_io.h"
#include "octaryn_client_app_log.h"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace octaryn_client_app {

namespace {

constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};
constexpr int kWorldSnapshotMinX = 0;
constexpr int kWorldSnapshotMaxXExclusive = 32;
constexpr int kWorldSnapshotMinZ = 0;
constexpr int kWorldSnapshotMaxZExclusive = 32;

uint64_t pack_column_key(int32_t x, int32_t z) {
  return static_cast<uint32_t>(x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(z)) << 32u);
}

bool is_spawn_column_block(const world_block_record &block) {
  return block.block != 0u && block.x >= kWorldSnapshotMinX &&
         block.x < kWorldSnapshotMaxXExclusive &&
         block.z >= kWorldSnapshotMinZ && block.z < kWorldSnapshotMaxZExclusive;
}

void apply_world_time_from_stream(const server_chunk_stream_file &stream,
                                  server_world_time_state &world_time) {
  world_time.active = true;
  world_time.day_index = stream.worldTimeDayIndex;
  world_time.second_of_day = stream.worldTimeSecondOfDay;
  world_time.total_seconds = stream.worldTimeTotalSeconds;
  world_time.day_fraction = std::clamp(stream.worldTimeDayFraction, 0.0f, 1.0f);
}

} // namespace

bool apply_top_blocks_from_records(
    const std::vector<world_block_record> &records, bool spawn_only,
    std::vector<presentation_block> &blocks) {
  std::unordered_map<uint64_t, presentation_block> top_blocks;
  for (const world_block_record &record : records) {
    if (record.block == 0u || (spawn_only && !is_spawn_column_block(record))) {
      continue;
    }

    const uint64_t key = pack_column_key(record.x, record.z);
    const auto iterator = top_blocks.find(key);
    if (iterator == top_blocks.end() || record.y > iterator->second.y) {
      top_blocks[key] =
          presentation_block{record.x, record.y, record.z, record.block};
    }
  }

  blocks.clear();
  blocks.reserve(top_blocks.size());
  for (const auto &entry : top_blocks) {
    blocks.push_back(entry.second);
  }

  std::sort(
      blocks.begin(), blocks.end(),
      [](const presentation_block &left, const presentation_block &right) {
        if (left.x != right.x) {
          return left.x < right.x;
        }

        return left.z < right.z;
      });
  return !blocks.empty();
}

bool apply_blocks_from_records(const std::vector<world_block_record> &records,
                               bool spawn_only,
                               std::vector<presentation_block> &blocks) {
  blocks.clear();
  blocks.reserve(records.size());
  for (const world_block_record &record : records) {
    if (record.block == 0u || (spawn_only && !is_spawn_column_block(record))) {
      continue;
    }

    blocks.push_back(
        presentation_block{record.x, record.y, record.z, record.block});
  }

  std::sort(
      blocks.begin(), blocks.end(),
      [](const presentation_block &left, const presentation_block &right) {
        if (left.x != right.x) {
          return left.x < right.x;
        }
        if (left.z != right.z) {
          return left.z < right.z;
        }

        return left.y < right.y;
      });
  return !blocks.empty();
}

bool load_world_snapshot_blocks(
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks,
    server_world_time_state &world_time) {
  const char *stream_path = std::getenv("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH");
  if (stream_path != nullptr && stream_path[0] != '\0') {
    std::string stream_payload;
    if (!read_text_file(stream_path, "server_chunk_stream_file=open_failed",
                        stream_payload)) {
      return false;
    }

    server_chunk_stream_file stream{};
    const auto stream_error =
        glz::read<kJsonReadOptions>(stream, stream_payload);
    if (stream_error) {
      log_line("server_chunk_stream_file=parse_failed");
      return false;
    }

    if (stream.version != 1 || stream.source != "server_process_chunk_stream") {
      log_line("server_chunk_stream_file=unsupported_version");
      return false;
    }

    apply_blocks_from_records(stream.blocks, false, snapshot_blocks);
    apply_top_blocks_from_records(stream.blocks, false, surface_blocks);
    apply_world_time_from_stream(stream, world_time);
    if (g_log != nullptr) {
      std::fprintf(g_log, "server_chunk_stream_loaded=%zu\n",
                   stream.blocks.size());
      std::fprintf(g_log, "server_chunk_stream_columns=%zu\n",
                   stream.columns.size());
      std::fprintf(g_log, "server_chunk_stream_surface_blocks_applied=%zu\n",
                   surface_blocks.size());
      std::fprintf(g_log,
                   "live_chunk_streaming active=1 source=server_process "
                   "epoch=%" PRIu64 " center=(%d,%d) radius=%" PRIu32
                   " columns=%zu loaded=%zu surface_blocks=%zu\n",
                   stream.epoch, stream.centerChunkX, stream.centerChunkZ,
                   stream.radius, stream.columns.size(), stream.blocks.size(),
                   surface_blocks.size());
      std::fprintf(g_log,
                   "live_sky_uniforms source=server_process day_fraction=%.6f "
                   "day_index=%" PRIu64 " second_of_day=%" PRIu32
                   " total_seconds=%.3f\n",
                   world_time.day_fraction, world_time.day_index,
                   world_time.second_of_day, world_time.total_seconds);
      std::fflush(g_log);
    }
    return !snapshot_blocks.empty();
  }

  const char *path = std::getenv("OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH");
  if (path == nullptr || path[0] == '\0') {
    log_line("live_chunk_streaming active=0 source=none surface_blocks=0 "
             "reason=no_runtime_chunk_streaming");
    return true;
  }

  std::string payload;
  if (!read_text_file(path, "world_blocks_file=open_failed", payload)) {
    return false;
  }

  world_block_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, payload);
  if (error) {
    log_line("world_blocks_file=parse_failed");
    return false;
  }

  if (file.version != 1) {
    log_line("world_blocks_file=unsupported_version");
    return false;
  }

  apply_blocks_from_records(file.blocks, true, snapshot_blocks);
  apply_top_blocks_from_records(file.blocks, true, surface_blocks);

  if (g_log != nullptr) {
    std::fprintf(g_log, "world_blocks_loaded=%zu\n", file.blocks.size());
    std::fprintf(g_log, "world_surface_blocks_applied=%zu\n",
                 surface_blocks.size());
    std::fprintf(g_log,
                 "live_chunk_streaming active=0 source=world_blocks_path "
                 "loaded=%zu surface_blocks=%zu reason=static_snapshot\n",
                 file.blocks.size(), surface_blocks.size());
    std::fflush(g_log);
  }
  return !snapshot_blocks.empty();
}

bool load_world_blocks_from_path(
    const std::filesystem::path &path,
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks) {
  std::string payload;
  if (!read_text_file(path.string().c_str(), "world_blocks_file=open_failed",
                      payload)) {
    return false;
  }

  world_block_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, payload);
  if (error || file.version != 1) {
    log_line("world_blocks_file=parse_failed");
    return false;
  }

  apply_blocks_from_records(file.blocks, true, snapshot_blocks);
  apply_top_blocks_from_records(file.blocks, true, surface_blocks);
  if (g_log != nullptr) {
    std::fprintf(g_log, "server_world_blocks_loaded=%zu surface_blocks=%zu\n",
                 file.blocks.size(), surface_blocks.size());
    std::fflush(g_log);
  }
  return !snapshot_blocks.empty();
}

bool load_server_chunk_stream_file(server_chunk_stream_file &stream,
                                   server_world_time_state &world_time,
                                   bool missing_is_waiting) {
  const char *stream_path = std::getenv("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH");
  if (stream_path == nullptr || stream_path[0] == '\0') {
    return false;
  }

  if (!std::filesystem::exists(stream_path)) {
    if (missing_is_waiting) {
      log_line("server_chunk_stream_file=waiting");
      return true;
    }

    log_line("server_chunk_stream_file=open_failed");
    return false;
  }

  std::string stream_payload;
  if (!read_text_file(stream_path, "server_chunk_stream_file=open_failed",
                      stream_payload)) {
    return false;
  }

  server_chunk_stream_file loaded{};
  const auto stream_error = glz::read<kJsonReadOptions>(loaded, stream_payload);
  if (stream_error) {
    if (missing_is_waiting) {
      log_line("server_chunk_stream_file=waiting reason=partial_write");
      return true;
    }

    log_line("server_chunk_stream_file=parse_failed");
    return false;
  }

  if (loaded.version != 1 || loaded.source != "server_process_chunk_stream") {
    log_line("server_chunk_stream_file=unsupported_version");
    return false;
  }

  stream = std::move(loaded);
  apply_world_time_from_stream(stream, world_time);
  if (g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_chunk_streaming active=1 source=server_background epoch=%" PRIu64
        " center=(%d,%d) radius=%" PRIu32
        " columns=%zu blocks=%zu world_time_day_fraction=%.6f\n",
        stream.epoch, stream.centerChunkX, stream.centerChunkZ, stream.radius,
        stream.columns.size(), stream.blocks.size(), world_time.day_fraction);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
