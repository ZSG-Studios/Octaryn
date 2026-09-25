#include "BlockStore.h"

#include <algorithm>
#include <tuple>

namespace octaryn::server::world::blocks {
namespace {

BlockEditResult unchanged() {
  return BlockEditResult{.applied = true, .changed = false, .edit = {}};
}

BlockEditResult invalid() {
  return BlockEditResult{.applied = false, .changed = false, .edit = {}};
}

BlockEditResult changed(const BlockEdit &edit) {
  return BlockEditResult{.applied = true, .changed = true, .edit = edit};
}

int32_t floor_div(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

int32_t floor_mod(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}

int32_t local_index_for(const BlockPosition &position) {
  return position.x + position.z * ChunkWidth +
         position.y * ChunkWidth * ChunkDepth;
}

BlockPosition local_position_from_index(int32_t index) {
  const int32_t layer = ChunkWidth * ChunkDepth;
  const int32_t y = index / layer;
  const int32_t remaining = index - y * layer;
  const int32_t z = remaining / ChunkWidth;
  const int32_t x = remaining - z * ChunkWidth;
  return BlockPosition{.x = x, .y = y, .z = z};
}

} // namespace

size_t
BlockStore::ChunkPositionHash::operator()(const ChunkPosition &position) const {
  const auto x = static_cast<uint32_t>(position.x);
  const auto y = static_cast<uint32_t>(position.y);
  const auto z = static_cast<uint32_t>(position.z);
  return static_cast<size_t>(x * 73856093u) ^
         static_cast<size_t>(y * 19349663u) ^
         static_cast<size_t>(z * 83492791u);
}

size_t BlockStore::block_count() const {
  size_t count = 0;
  for (const auto &entry : chunks_) {
    count += entry.second.size();
  }
  return count;
}

uint16_t BlockStore::get_block(const BlockPosition &position) const {
  uint16_t block = AirBlock;
  return try_get_block(position, block) ? block : AirBlock;
}

bool BlockStore::try_get_block(const BlockPosition &position,
                               uint16_t &block) const {
  block = AirBlock;
  if (!is_valid_position(position)) {
    return false;
  }

  const auto chunk = chunks_.find(chunk_position_for(position));
  if (chunk == chunks_.end()) {
    return false;
  }

  const auto local_block =
      chunk->second.find(local_index_for(local_position_for(position)));
  if (local_block == chunk->second.end()) {
    return false;
  }

  block = local_block->second;
  return true;
}

BlockEditResult
BlockStore::clear_block_override(const BlockPosition &position) {
  if (!is_valid_position(position)) {
    return invalid();
  }

  const ChunkPosition chunk_position = chunk_position_for(position);
  auto chunk = chunks_.find(chunk_position);
  if (chunk == chunks_.end()) {
    return unchanged();
  }

  const int32_t index = local_index_for(local_position_for(position));
  if (chunk->second.erase(index) == 0) {
    return unchanged();
  }

  if (chunk->second.empty()) {
    chunks_.erase(chunk);
  }

  return changed(BlockEdit{.position = position, .block = AirBlock});
}

BlockEditResult BlockStore::set_block(const BlockEdit &edit,
                                      bool preserve_air_override) {
  if (!is_valid_position(edit.position)) {
    return invalid();
  }

  const ChunkPosition chunk_position = chunk_position_for(edit.position);
  auto chunk = chunks_.find(chunk_position);
  if (chunk == chunks_.end()) {
    if (edit.block == AirBlock && !preserve_air_override) {
      return unchanged();
    }
    chunk = chunks_.emplace(chunk_position, LocalBlocks{}).first;
  }

  const int32_t index = local_index_for(local_position_for(edit.position));
  const auto existing = chunk->second.find(index);
  const bool has_existing = existing != chunk->second.end();
  const uint16_t old_block = has_existing ? existing->second : AirBlock;
  if (old_block == edit.block && !(preserve_air_override && !has_existing)) {
    return unchanged();
  }

  if (edit.block == AirBlock && !preserve_air_override) {
    chunk->second.erase(index);
  } else {
    chunk->second[index] = edit.block;
  }

  if (chunk->second.empty()) {
    chunks_.erase(chunk);
  }

  return changed(edit);
}

std::vector<BlockEdit> BlockStore::snapshot() const {
  std::vector<BlockEdit> edits;
  edits.reserve(block_count());

  std::vector<ChunkPosition> chunk_positions;
  chunk_positions.reserve(chunks_.size());
  for (const auto &entry : chunks_) {
    chunk_positions.push_back(entry.first);
  }
  std::sort(chunk_positions.begin(), chunk_positions.end(),
            [](const ChunkPosition &left, const ChunkPosition &right) {
              return std::tie(left.x, left.y, left.z) <
                     std::tie(right.x, right.y, right.z);
            });

  for (const ChunkPosition &chunk_position : chunk_positions) {
    const auto chunk = chunks_.find(chunk_position);
    if (chunk == chunks_.end()) {
      continue;
    }

    std::vector<int32_t> local_indices;
    local_indices.reserve(chunk->second.size());
    for (const auto &block : chunk->second) {
      local_indices.push_back(block.first);
    }
    std::sort(local_indices.begin(), local_indices.end());

    for (const int32_t index : local_indices) {
      const BlockPosition local = local_position_from_index(index);
      edits.push_back(BlockEdit{
          .position =
              BlockPosition{
                  .x = chunk_position.x * ChunkWidth + local.x,
                  .y = chunk_position.y * ChunkSectionHeight + local.y,
                  .z = chunk_position.z * ChunkDepth + local.z,
              },
          .block = chunk->second.at(index),
      });
    }
  }

  return edits;
}

std::vector<BlockEdit>
BlockStore::snapshot_chunk_column(int32_t origin_x, int32_t origin_z) const {
  std::vector<BlockEdit> edits;
  const int32_t chunk_x = floor_div(origin_x, ChunkWidth);
  const int32_t chunk_z = floor_div(origin_z, ChunkDepth);
  for (int32_t chunk_y = floor_div(WorldMinY, ChunkSectionHeight);
       chunk_y < floor_div(WorldMaxYExclusive - 1, ChunkSectionHeight) + 1;
       ++chunk_y) {
    const ChunkPosition chunk_position{.x = chunk_x,
                                       .y = chunk_y,
                                       .z = chunk_z};
    const auto chunk = chunks_.find(chunk_position);
    if (chunk == chunks_.end()) {
      continue;
    }

    std::vector<int32_t> local_indices;
    local_indices.reserve(chunk->second.size());
    for (const auto &block : chunk->second) {
      local_indices.push_back(block.first);
    }
    std::sort(local_indices.begin(), local_indices.end());

    for (const int32_t index : local_indices) {
      const BlockPosition local = local_position_from_index(index);
      edits.push_back(BlockEdit{
          .position =
              BlockPosition{
                  .x = chunk_position.x * ChunkWidth + local.x,
                  .y = chunk_position.y * ChunkSectionHeight + local.y,
                  .z = chunk_position.z * ChunkDepth + local.z,
              },
          .block = chunk->second.at(index),
      });
    }
  }
  return edits;
}

void BlockStore::load(const std::vector<BlockEdit> &edits) {
  chunks_.clear();
  for (const BlockEdit &edit : edits) {
    set_block(edit, edit.block == AirBlock);
  }
}

int32_t BlockStore::clear_overrides_matching(
    const std::function<uint16_t(const BlockPosition &)> &generated_block) {
  int32_t cleared = 0;
  for (const BlockEdit &edit : snapshot()) {
    if (generated_block(edit.position) == edit.block &&
        clear_block_override(edit.position).changed) {
      ++cleared;
    }
  }
  return cleared;
}

bool is_valid_position(const BlockPosition &position) {
  return position.y >= WorldMinY && position.y < WorldMaxYExclusive;
}

ChunkPosition chunk_position_for(const BlockPosition &position) {
  return ChunkPosition{
      .x = floor_div(position.x, ChunkWidth),
      .y = floor_div(position.y, ChunkSectionHeight),
      .z = floor_div(position.z, ChunkDepth),
  };
}

BlockPosition local_position_for(const BlockPosition &position) {
  return BlockPosition{
      .x = floor_mod(position.x, ChunkWidth),
      .y = floor_mod(position.y, ChunkSectionHeight),
      .z = floor_mod(position.z, ChunkDepth),
  };
}

} // namespace octaryn::server::world::blocks
