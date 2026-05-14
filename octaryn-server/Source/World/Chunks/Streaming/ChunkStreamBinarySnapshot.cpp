#include "ChunkStreamBinarySnapshot.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr char kMagic[8] = {'O', 'C', 'S', 'T', 'R', 'M', '0', '1'};
constexpr uint32_t kVersion = 1u;

std::filesystem::path binary_path_for(const char *stream_path) {
  return std::filesystem::path{std::string(stream_path) + ".bin"};
}

template <typename T> bool write_value(std::ofstream &output, const T &value) {
  output.write(reinterpret_cast<const char *>(&value), sizeof(T));
  return static_cast<bool>(output);
}

bool write_header(std::ofstream &output,
                  const octaryn_server_chunk_stream_snapshot_request &request,
                  uint32_t column_count, uint32_t block_count) {
  output.write(kMagic, sizeof(kMagic));
  return output && write_value(output, kVersion) &&
         write_value(output, request.epoch) &&
         write_value(output, request.center_chunk_x) &&
         write_value(output, request.center_chunk_z) &&
         write_value(output, request.radius) &&
         write_value(output, request.world_seed) &&
         write_value(output, request.world_time_day_index) &&
         write_value(output, request.world_time_second_of_day) &&
         write_value(output, request.world_time_total_seconds) &&
         write_value(output, request.world_time_day_fraction) &&
         write_value(output, request.player_x) &&
         write_value(output, request.player_y) &&
         write_value(output, request.player_z) &&
         write_value(output, request.player_pitch) &&
         write_value(output, request.player_yaw) &&
         write_value(output, request.player_velocity_x) &&
         write_value(output, request.player_velocity_y) &&
         write_value(output, request.player_velocity_z) &&
         write_value(output, request.player_control_mode) &&
         write_value(output, request.player_on_ground) &&
         write_value(output, column_count) && write_value(output, block_count);
}

bool write_columns(
    std::ofstream &output,
    const std::vector<octaryn_server_chunk_stream_column> &columns) {
  for (const auto &column : columns) {
    if (!write_value(output, column.chunk_x) ||
        !write_value(output, column.chunk_z) ||
        !write_value(output, column.origin_x) ||
        !write_value(output, column.origin_z) ||
        !write_value(output, column.block_offset) ||
        !write_value(output, column.block_count)) {
      return false;
    }
  }
  return true;
}

bool write_blocks(
    std::ofstream &output,
    const std::vector<octaryn_server_chunk_stream_block> &blocks) {
  for (const auto &block : blocks) {
    if (!write_value(output, block.x) || !write_value(output, block.y) ||
        !write_value(output, block.z) || !write_value(output, block.block)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool write_chunk_stream_binary_snapshot(
    const char *stream_path,
    const octaryn_server_chunk_stream_snapshot_request &request,
    const std::vector<octaryn_server_chunk_stream_column> &columns,
    const std::vector<octaryn_server_chunk_stream_block> &blocks) {
  if (stream_path == nullptr || stream_path[0] == '\0') {
    return false;
  }

  const std::filesystem::path output_path = binary_path_for(stream_path);
  if (output_path.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(output_path.parent_path(), error);
    if (error) {
      return false;
    }
  }

  const std::filesystem::path temporary_path{output_path.string() + ".tmp"};
  std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
  if (!output) {
    return false;
  }

  const bool write_ok =
      write_header(output, request, static_cast<uint32_t>(columns.size()),
                   static_cast<uint32_t>(blocks.size())) &&
      write_columns(output, columns) && write_blocks(output, blocks);
  output.close();
  if (!write_ok || !output) {
    return false;
  }

  std::error_code error;
  std::filesystem::rename(temporary_path, output_path, error);
  if (!error) {
    return true;
  }

  std::filesystem::remove(output_path, error);
  error.clear();
  std::filesystem::rename(temporary_path, output_path, error);
  return !error;
}
