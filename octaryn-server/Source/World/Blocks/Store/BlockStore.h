#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#if !defined(OCTARYN_SERVER_BLOCK_STORE_API)
#if defined(_WIN32)
#define OCTARYN_SERVER_BLOCK_STORE_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_BLOCK_STORE_API __attribute__((visibility("default")))
#endif
#endif

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

extern "C" {

struct octaryn_server_block_position {
  int32_t x;
  int32_t y;
  int32_t z;
};

struct octaryn_server_chunk_position {
  int32_t x;
  int32_t y;
  int32_t z;
};

struct octaryn_server_block_edit {
  octaryn_server_block_position position;
  uint16_t block;
};

struct octaryn_server_block_edit_result {
  uint32_t applied;
  uint32_t changed;
  octaryn_server_block_edit edit;
};

using octaryn_server_generated_block_fn =
    uint16_t (*)(void *context, const octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API void *octaryn_server_block_store_create();

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_block_store_destroy(void *store);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_store_block_count(void *store);

OCTARYN_SERVER_BLOCK_STORE_API uint16_t
octaryn_server_block_store_get_block(
    void *store, const octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_block_store_try_get_block(
    void *store, const octaryn_server_block_position *position,
    uint16_t *block);

OCTARYN_SERVER_BLOCK_STORE_API octaryn_server_block_edit_result
octaryn_server_block_store_clear_block_override(
    void *store, const octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API octaryn_server_block_edit_result
octaryn_server_block_store_set_block(void *store,
                                     const octaryn_server_block_edit *edit,
                                     uint32_t preserve_air_override);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_store_snapshot_count(void *store);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_store_snapshot_fill(void *store,
                                         octaryn_server_block_edit *edits,
                                         uint64_t capacity);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_store_snapshot_chunk_column_count(void *store,
                                                       int32_t origin_x,
                                                       int32_t origin_z);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_store_snapshot_chunk_column_fill(
    void *store, int32_t origin_x, int32_t origin_z,
    octaryn_server_block_edit *edits, uint64_t capacity);

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_block_store_load(void *store,
                                const octaryn_server_block_edit *edits,
                                uint64_t count);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_block_store_clear_overrides_matching(
    void *store, octaryn_server_generated_block_fn generated_block,
    void *context);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_block_store_is_valid_position(
    const octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API octaryn_server_chunk_position
octaryn_server_block_store_chunk_position_for(
    const octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API octaryn_server_block_position
octaryn_server_block_store_local_position_for(
    const octaryn_server_block_position *position);

}
