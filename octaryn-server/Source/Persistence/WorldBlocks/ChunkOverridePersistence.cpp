#include "WorldPersistence.h"

#include "BlockStore.h"
#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <charconv>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace octaryn::server::persistence {

struct chunk_override_block_file {
  int32_t bx = 0;
  int32_t by = 0;
  int32_t bz = 0;
  uint16_t block = 0u;
};

struct chunk_override_file {
  uint32_t version = 2u;
  int32_t cx = 0;
  int32_t cz = 0;
  std::vector<chunk_override_block_file> blocks{};
};

struct world_block_override_block_file {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
  uint16_t block = 0u;
};

struct world_block_override_file {
  uint32_t version = 1u;
  std::vector<world_block_override_block_file> blocks{};
};

} // namespace octaryn::server::persistence

namespace {

using octaryn::server::persistence::chunk_override_block_file;
using octaryn::server::persistence::chunk_override_file;
using octaryn::server::persistence::world_block_override_block_file;
using octaryn::server::persistence::world_block_override_file;
using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;

constexpr uint32_t CurrentChunkOverrideVersion = 2u;
constexpr uint32_t CurrentWorldOverrideVersion = 1u;
constexpr uint32_t LegacyLocalCoordinateVersion = 1u;
constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts JsonWriteOptions{.prettify = true};

bool upgrade_chunk_override_file(chunk_override_file &file) {
  if (file.version == CurrentChunkOverrideVersion) {
    return true;
  }

  if (file.version != LegacyLocalCoordinateVersion) {
    return false;
  }

  bool saw_local_only = false;
  bool saw_world_only = false;
  for (const auto &block : file.blocks) {
    const bool already_world_coordinates =
        block.bx >= file.cx - 1 && block.bx <= file.cx + ChunkWidth &&
        block.bz >= file.cz - 1 && block.bz <= file.cz + ChunkDepth;
    const bool looks_like_local_coordinates =
        block.bx >= -1 && block.bx <= ChunkWidth && block.bz >= -1 &&
        block.bz <= ChunkDepth;

    if (!already_world_coordinates && !looks_like_local_coordinates) {
      return false;
    }

    saw_local_only |=
        looks_like_local_coordinates && !already_world_coordinates;
    saw_world_only |=
        already_world_coordinates && !looks_like_local_coordinates;
  }

  if ((saw_local_only && saw_world_only) ||
      (!saw_local_only && !saw_world_only && !file.blocks.empty())) {
    return false;
  }

  if (saw_local_only) {
    for (auto &block : file.blocks) {
      block.bx += file.cx;
      block.bz += file.cz;
    }
  }

  file.version = CurrentChunkOverrideVersion;
  return true;
}

bool read_chunk_override_file(const char *path, chunk_override_file &file) {
  const std::filesystem::path file_path(path);
  if (!std::filesystem::exists(file_path)) {
    return false;
  }

  std::string payload;
  if (!octaryn::server::persistence::read_text_file(file_path, payload) ||
      glz::read<JsonReadOptions>(file, payload) ||
      !upgrade_chunk_override_file(file)) {
    return false;
  }

  return true;
}

bool read_world_block_override_file(const char *path,
                                    world_block_override_file &file) {
  const std::filesystem::path file_path(path);
  if (!std::filesystem::exists(file_path)) {
    return false;
  }

  std::string payload;
  if (!octaryn::server::persistence::read_text_file(file_path, payload) ||
      glz::read<JsonReadOptions>(file, payload) ||
      file.version != CurrentWorldOverrideVersion) {
    return false;
  }

  return true;
}

bool parse_chunk_column_filename(const std::filesystem::path &path,
                                 int32_t &origin_x, int32_t &origin_z) {
  const std::string name = path.stem().string();
  constexpr std::string_view Prefix = "chunk_";
  if (!std::string_view(name).starts_with(Prefix)) {
    return false;
  }

  const std::string_view coordinates(name.data() + Prefix.size(),
                                     name.size() - Prefix.size());
  const size_t separator = coordinates.find('_');
  if (separator == std::string_view::npos) {
    return false;
  }

  const std::string_view x_text = coordinates.substr(0u, separator);
  const std::string_view z_text = coordinates.substr(separator + 1u);
  if (x_text.empty() || z_text.empty()) {
    return false;
  }

  int32_t parsed_x = 0;
  int32_t parsed_z = 0;
  const auto *x_begin = x_text.data();
  const auto *x_end = x_text.data() + x_text.size();
  const auto *z_begin = z_text.data();
  const auto *z_end = z_text.data() + z_text.size();
  const auto x_result = std::from_chars(x_begin, x_end, parsed_x);
  const auto z_result = std::from_chars(z_begin, z_end, parsed_z);
  if (x_result.ec != std::errc{} || x_result.ptr != x_end ||
      z_result.ec != std::errc{} || z_result.ptr != z_end) {
    return false;
  }

  origin_x = parsed_x;
  origin_z = parsed_z;
  return true;
}

octaryn_server_persistence_chunk_override_file
abi_file_from_file(const chunk_override_file &file) {
  return octaryn_server_persistence_chunk_override_file{
      .version = file.version,
      .cx = file.cx,
      .cz = file.cz,
      .block_count = static_cast<uint32_t>(file.blocks.size()),
  };
}

chunk_override_file
file_from_abi(const octaryn_server_persistence_chunk_override_file &file,
              const octaryn_server_persistence_chunk_override_block *blocks) {
  chunk_override_file result{
      .version = file.version,
      .cx = file.cx,
      .cz = file.cz,
      .blocks = {},
  };
  result.blocks.reserve(file.block_count);
  for (uint32_t index = 0u; index < file.block_count; ++index) {
    result.blocks.push_back(chunk_override_block_file{
        .bx = blocks[index].bx,
        .by = blocks[index].by,
        .bz = blocks[index].bz,
        .block = blocks[index].block,
    });
  }
  return result;
}

octaryn_server_persistence_world_block_override_file
abi_file_from_file(const world_block_override_file &file) {
  return octaryn_server_persistence_world_block_override_file{
      .version = file.version,
      .block_count = static_cast<uint32_t>(file.blocks.size()),
  };
}

world_block_override_file world_file_from_abi(
    const octaryn_server_persistence_world_block_override_file &file,
    const octaryn_server_persistence_block_edit *blocks) {
  world_block_override_file result{
      .version = file.version,
      .blocks = {},
  };
  result.blocks.reserve(file.block_count);
  for (uint32_t index = 0u; index < file.block_count; ++index) {
    result.blocks.push_back(world_block_override_block_file{
        .x = blocks[index].position.x,
        .y = blocks[index].position.y,
        .z = blocks[index].position.z,
        .block = blocks[index].block,
    });
  }
  return result;
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_chunk_override_file_count(
    const char *path, octaryn_server_persistence_chunk_override_file *file) {
  if (path == nullptr || path[0] == '\0' || file == nullptr) {
    return -1;
  }

  if (!std::filesystem::exists(std::filesystem::path(path))) {
    return 1;
  }

  chunk_override_file loaded{};
  if (!read_chunk_override_file(path, loaded)) {
    return -2;
  }

  *file = abi_file_from_file(loaded);
  return 0;
}

int32_t octaryn_server_persistence_read_chunk_override_file_fill(
    const char *path, octaryn_server_persistence_chunk_override_block *blocks,
    uint32_t block_capacity,
    octaryn_server_persistence_chunk_override_file *file) {
  if (path == nullptr || path[0] == '\0' || file == nullptr) {
    return -1;
  }

  chunk_override_file loaded{};
  if (!read_chunk_override_file(path, loaded)) {
    return std::filesystem::exists(std::filesystem::path(path)) ? -2 : 1;
  }

  if (block_capacity < loaded.blocks.size()) {
    return -3;
  }
  if (!loaded.blocks.empty() && blocks == nullptr) {
    return -1;
  }

  for (uint32_t index = 0u; index < loaded.blocks.size(); ++index) {
    const auto &block = loaded.blocks[index];
    blocks[index] = octaryn_server_persistence_chunk_override_block{
        .bx = block.bx,
        .by = block.by,
        .bz = block.bz,
        .block = block.block,
    };
  }

  *file = abi_file_from_file(loaded);
  return 0;
}

int32_t octaryn_server_persistence_write_chunk_override_file(
    const char *path,
    const octaryn_server_persistence_chunk_override_file *file,
    const octaryn_server_persistence_chunk_override_block *blocks) {
  if (path == nullptr || path[0] == '\0' || file == nullptr ||
      (file->block_count != 0u && blocks == nullptr)) {
    return -1;
  }

  const chunk_override_file output = file_from_abi(*file, blocks);

  std::string payload;
  if (glz::write<JsonWriteOptions>(output, payload)) {
    return -2;
  }

  return octaryn::server::persistence::write_text_file_atomically(
             std::filesystem::path(path), payload)
             ? 0
             : -3;
}

int32_t octaryn_server_persistence_read_world_block_override_file_count(
    const char *path,
    octaryn_server_persistence_world_block_override_file *file) {
  if (path == nullptr || path[0] == '\0' || file == nullptr) {
    return -1;
  }

  if (!std::filesystem::exists(std::filesystem::path(path))) {
    return 1;
  }

  world_block_override_file loaded{};
  if (!read_world_block_override_file(path, loaded)) {
    return -2;
  }

  *file = abi_file_from_file(loaded);
  return 0;
}

int32_t octaryn_server_persistence_read_world_block_override_file_fill(
    const char *path, octaryn_server_persistence_block_edit *blocks,
    uint32_t block_capacity,
    octaryn_server_persistence_world_block_override_file *file) {
  if (path == nullptr || path[0] == '\0' || file == nullptr) {
    return -1;
  }

  world_block_override_file loaded{};
  if (!read_world_block_override_file(path, loaded)) {
    return std::filesystem::exists(std::filesystem::path(path)) ? -2 : 1;
  }

  if (block_capacity < loaded.blocks.size()) {
    return -3;
  }
  if (!loaded.blocks.empty() && blocks == nullptr) {
    return -1;
  }

  for (uint32_t index = 0u; index < loaded.blocks.size(); ++index) {
    const auto &block = loaded.blocks[index];
    blocks[index] = octaryn_server_persistence_block_edit{
        .position = {.x = block.x, .y = block.y, .z = block.z},
        .block = block.block,
    };
  }

  *file = abi_file_from_file(loaded);
  return 0;
}

int32_t octaryn_server_persistence_write_world_block_override_file(
    const char *path,
    const octaryn_server_persistence_world_block_override_file *file,
    const octaryn_server_persistence_block_edit *blocks) {
  if (path == nullptr || path[0] == '\0' || file == nullptr ||
      (file->block_count != 0u && blocks == nullptr)) {
    return -1;
  }

  const world_block_override_file output = world_file_from_abi(*file, blocks);

  std::string payload;
  if (glz::write<JsonWriteOptions>(output, payload)) {
    return -2;
  }

  return octaryn::server::persistence::write_text_file_atomically(
             std::filesystem::path(path), payload)
             ? 0
             : -3;
}

int32_t octaryn_server_persistence_scan_chunk_override_directory(
    const char *directory, const char *aggregate_path,
    octaryn_server_persistence_chunk_override_directory_scan *scan) {
  if (directory == nullptr || directory[0] == '\0' || scan == nullptr) {
    return -1;
  }

  *scan = octaryn_server_persistence_chunk_override_directory_scan{};
  const std::filesystem::path root(directory);
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) {
    return 0;
  }

  const std::filesystem::path aggregate =
      aggregate_path == nullptr ? std::filesystem::path{} : aggregate_path;
  const bool aggregate_exists =
      !aggregate.empty() && std::filesystem::exists(aggregate, error) && !error;
  error.clear();
  const auto aggregate_time =
      aggregate_exists ? std::filesystem::last_write_time(aggregate, error)
                       : std::filesystem::file_time_type::min();
  if (error) {
    return -2;
  }

  std::set<std::pair<int32_t, int32_t>> origins{};
  uint32_t block_count = 0u;
  bool has_current_file = false;
  for (const auto &entry : std::filesystem::directory_iterator(root, error)) {
    if (error) {
      return -2;
    }

    std::error_code entry_error;
    if (!entry.is_regular_file(entry_error) || entry_error) {
      continue;
    }

    int32_t origin_x = 0;
    int32_t origin_z = 0;
    if (!parse_chunk_column_filename(entry.path(), origin_x, origin_z)) {
      continue;
    }

    chunk_override_file file{};
    const std::string path = entry.path().string();
    if (!read_chunk_override_file(path.c_str(), file) || file.cx != origin_x ||
        file.cz != origin_z) {
      continue;
    }

    origins.emplace(origin_x, origin_z);
    block_count += static_cast<uint32_t>(file.blocks.size());
    if (!aggregate_exists) {
      has_current_file = true;
    } else {
      const auto entry_time =
          std::filesystem::last_write_time(entry.path(), entry_error);
      if (!entry_error && entry_time >= aggregate_time) {
        has_current_file = true;
      }
    }
  }

  scan->current_files_at_least_as_new_as = has_current_file ? 1u : 0u;
  scan->file_count = static_cast<uint32_t>(origins.size());
  scan->block_count = block_count;
  return 0;
}
}
