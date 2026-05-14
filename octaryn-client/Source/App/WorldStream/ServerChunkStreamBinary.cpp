#include "ServerChunkStreamBinary.h"

#include <array>
#include <cstring>
#include <fstream>

namespace {

constexpr std::array<char, 8> kMagic{'O', 'C', 'S', 'T', 'R', 'M', '0', '1'};
constexpr uint32_t kVersion = 1u;

std::filesystem::path binary_path_for(
    const std::filesystem::path &json_stream_path) {
  return std::filesystem::path{json_stream_path.string() + ".bin"};
}

template <typename T> bool read_value(std::ifstream &input, T &value) {
  input.read(reinterpret_cast<char *>(&value), sizeof(T));
  return static_cast<bool>(input);
}

bool read_header(std::ifstream &input,
                 octaryn_client_app::server_chunk_stream_file &stream,
                 uint32_t &column_count, uint32_t &block_count) {
  std::array<char, 8> magic{};
  input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  uint32_t version = 0u;
  if (!input || magic != kMagic || !read_value(input, version) ||
      version != kVersion) {
    return false;
  }

  stream.version = 1;
  stream.source = "server_process_chunk_stream";
  return read_value(input, stream.epoch) &&
         read_value(input, stream.centerChunkX) &&
         read_value(input, stream.centerChunkZ) &&
         read_value(input, stream.radius) &&
         read_value(input, stream.worldSeed) &&
         read_value(input, stream.worldTimeDayIndex) &&
         read_value(input, stream.worldTimeSecondOfDay) &&
         read_value(input, stream.worldTimeTotalSeconds) &&
         read_value(input, stream.worldTimeDayFraction) &&
         read_value(input, stream.playerX) &&
         read_value(input, stream.playerY) &&
         read_value(input, stream.playerZ) &&
         read_value(input, stream.playerPitch) &&
         read_value(input, stream.playerYaw) &&
         read_value(input, stream.playerVelocityX) &&
         read_value(input, stream.playerVelocityY) &&
         read_value(input, stream.playerVelocityZ) &&
         read_value(input, stream.playerControlMode) &&
         read_value(input, stream.playerOnGround) &&
         read_value(input, column_count) && read_value(input, block_count);
}

bool read_columns(
    std::ifstream &input,
    std::vector<octaryn_client_app::server_chunk_stream_column_record>
        &columns) {
  for (auto &column : columns) {
    if (!read_value(input, column.chunkX) ||
        !read_value(input, column.chunkZ) ||
        !read_value(input, column.originX) ||
        !read_value(input, column.originZ) ||
        !read_value(input, column.blockOffset) ||
        !read_value(input, column.blockCount)) {
      return false;
    }
  }
  return true;
}

bool read_blocks(std::ifstream &input,
                 std::vector<octaryn_client_app::world_block_record> &blocks) {
  for (auto &block : blocks) {
    if (!read_value(input, block.x) || !read_value(input, block.y) ||
        !read_value(input, block.z) || !read_value(input, block.block)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool load_server_chunk_stream_binary_file(
    const std::filesystem::path &json_stream_path,
    octaryn_client_app::server_chunk_stream_file &stream) {
  const std::filesystem::path binary_path = binary_path_for(json_stream_path);
  std::ifstream input{binary_path, std::ios::binary};
  if (!input) {
    return false;
  }

  octaryn_client_app::server_chunk_stream_file loaded{};
  uint32_t column_count = 0u;
  uint32_t block_count = 0u;
  if (!read_header(input, loaded, column_count, block_count)) {
    return false;
  }

  loaded.columns.resize(column_count);
  loaded.blocks.resize(block_count);
  if (!read_columns(input, loaded.columns) || !read_blocks(input, loaded.blocks)) {
    return false;
  }

  char trailing = '\0';
  if (input.read(&trailing, 1)) {
    return false;
  }

  stream = std::move(loaded);
  return true;
}
