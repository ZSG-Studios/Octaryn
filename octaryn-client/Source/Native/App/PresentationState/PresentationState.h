#pragma once

#include "octaryn_client_host_exports.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace octaryn_client_app {

struct presentation_block {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct block_position_key {
  int32_t x;
  int32_t y;
  int32_t z;

  bool operator==(const block_position_key &other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct block_position_key_hash {
  size_t operator()(const block_position_key &key) const {
    uint64_t value = static_cast<uint32_t>(key.x);
    value = value * 1099511628211ull ^ static_cast<uint32_t>(key.y);
    value = value * 1099511628211ull ^ static_cast<uint32_t>(key.z);
    return static_cast<size_t>(value);
  }
};

using block_lookup =
    std::unordered_map<block_position_key, uint16_t, block_position_key_hash>;

uint64_t pack_signed_pair(int32_t a, int32_t b);
uint64_t pack_block(int32_t z, uint16_t block);
int32_t unpack_low(uint64_t value);
int32_t unpack_high(uint64_t value);
block_lookup build_block_lookup(const std::vector<presentation_block> &blocks);
uint16_t find_block(const block_lookup &lookup, const block_position_key &key);
bool has_block_override(const block_lookup &lookup, const block_position_key &key,
                        uint16_t &block);
void apply_local_block_record(std::vector<presentation_block> &blocks,
                              const presentation_block &update);
int apply_snapshot_blocks(const std::vector<presentation_block> &blocks,
                          uint64_t tick_id);
bool drain_presentation_updates(std::vector<presentation_block> &blocks,
                                uint32_t &written);

} // namespace octaryn_client_app
