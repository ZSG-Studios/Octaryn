#include "WorldPersistence.h"

#include "BlockStore.h"
#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>
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
}
