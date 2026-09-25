#include "ChunkColumnStream.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace octaryn::server::world::chunks {

struct chunk_view_intent_file {
  int32_t version = 0;
  uint64_t epoch = 0u;
  int32_t centerChunkX = 0;
  int32_t centerChunkZ = 0;
  uint32_t radius = 0u;
  bool hasPreviousWindow = false;
  int32_t previousCenterChunkX = 0;
  int32_t previousCenterChunkZ = 0;
  uint32_t previousRadius = 0u;
};

} // namespace octaryn::server::world::chunks

namespace {

constexpr uint32_t MaxRequestRadius = 32u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};

using octaryn::server::world::chunks::chunk_view_intent_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

octaryn_server_chunk_view_intent to_native_intent(
    const chunk_view_intent_file &file) {
  return octaryn_server_chunk_view_intent{
      .version = file.version,
      .epoch = file.epoch,
      .center_chunk_x = file.centerChunkX,
      .center_chunk_z = file.centerChunkZ,
      .radius = file.radius,
      .has_previous_window = file.hasPreviousWindow ? 1u : 0u,
      .previous_center_chunk_x = file.previousCenterChunkX,
      .previous_center_chunk_z = file.previousCenterChunkZ,
      .previous_radius = file.previousRadius,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_chunk_stream_read_view_intent(
    const char *intent_path, octaryn_server_chunk_view_intent *intent) {
  if (intent_path == nullptr || intent_path[0] == '\0' || intent == nullptr) {
    return -1;
  }

  const std::filesystem::path path{intent_path};
  if (!std::filesystem::exists(path)) {
    return 1;
  }

  std::string payload;
  if (!read_text_file(path, payload)) {
    return -2;
  }

  chunk_view_intent_file file{};
  if (glz::read<JsonReadOptions>(file, payload)) {
    return -3;
  }

  if (file.version != 1 || file.radius > MaxRequestRadius) {
    return -4;
  }

  *intent = to_native_intent(file);
  return 0;
}

}
