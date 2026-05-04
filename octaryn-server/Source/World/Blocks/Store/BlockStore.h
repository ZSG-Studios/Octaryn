#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace octaryn::server::world::blocks {

inline constexpr int32_t ChunkWidth = 32;
inline constexpr int32_t ChunkDepth = 32;
inline constexpr int32_t ChunkSectionHeight = 32;
inline constexpr int32_t WorldHeight = 512;
inline constexpr int32_t WorldMinY = -WorldHeight / 2;
inline constexpr int32_t WorldMaxYExclusive = WorldMinY + WorldHeight;
inline constexpr uint16_t AirBlock = 0;

struct BlockPosition {
  int32_t x;
  int32_t y;
  int32_t z;

  friend bool operator==(const BlockPosition &left,
                         const BlockPosition &right) = default;
};

struct ChunkPosition {
  int32_t x;
  int32_t y;
  int32_t z;

  friend bool operator==(const ChunkPosition &left,
                         const ChunkPosition &right) = default;
};

struct BlockEdit {
  BlockPosition position;
  uint16_t block;
};

struct BlockEditResult {
  bool applied;
  bool changed;
  BlockEdit edit;
};

class BlockStore {
public:
  [[nodiscard]] size_t block_count() const;
  [[nodiscard]] uint16_t get_block(const BlockPosition &position) const;
  [[nodiscard]] bool try_get_block(const BlockPosition &position,
                                   uint16_t &block) const;

  BlockEditResult clear_block_override(const BlockPosition &position);
  BlockEditResult set_block(const BlockEdit &edit,
                            bool preserve_air_override = false);

  [[nodiscard]] std::vector<BlockEdit> snapshot() const;
  [[nodiscard]] std::vector<BlockEdit>
  snapshot_chunk_column(int32_t origin_x, int32_t origin_z) const;

  void load(const std::vector<BlockEdit> &edits);
  int32_t clear_overrides_matching(
      const std::function<uint16_t(const BlockPosition &)> &generated_block);

private:
  struct ChunkPositionHash {
    [[nodiscard]] size_t operator()(const ChunkPosition &position) const;
  };

  using LocalBlocks = std::unordered_map<int32_t, uint16_t>;
  std::unordered_map<ChunkPosition, LocalBlocks, ChunkPositionHash> chunks_;
};

[[nodiscard]] bool is_valid_position(const BlockPosition &position);
[[nodiscard]] ChunkPosition chunk_position_for(const BlockPosition &position);
[[nodiscard]] BlockPosition local_position_for(const BlockPosition &position);

} // namespace octaryn::server::world::blocks
