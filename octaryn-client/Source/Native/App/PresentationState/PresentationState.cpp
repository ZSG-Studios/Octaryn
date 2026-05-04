#include "PresentationState.h"

#include "Log.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>

namespace octaryn_client_app {

namespace {

constexpr int kMaxPresentationUpdatesPerFrame = 256;

void apply_presentation_update(std::vector<presentation_block> &blocks,
                               const octaryn_replication_change &change) {
  if (change.version != 1u || change.size != OCTARYN_REPLICATION_CHANGE_SIZE ||
      change.change_kind != 1u) {
    return;
  }

  presentation_block update{};
  update.x = unpack_low(change.payload0);
  update.y = unpack_high(change.payload0);
  update.z = unpack_low(change.payload1);
  update.block = static_cast<uint16_t>(change.payload1 >> 32u);

  apply_local_block_record(blocks, update);
}

} // namespace

uint64_t pack_signed_pair(int32_t a, int32_t b) {
  return static_cast<uint32_t>(a) |
         (static_cast<uint64_t>(static_cast<uint32_t>(b)) << 32u);
}

uint64_t pack_block(int32_t z, uint16_t block) {
  return static_cast<uint32_t>(z) | (static_cast<uint64_t>(block) << 32u);
}

int32_t unpack_low(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int32_t unpack_high(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value >> 32u));
}

block_lookup build_block_lookup(const std::vector<presentation_block> &blocks) {
  block_lookup lookup;
  lookup.reserve(blocks.size());
  for (const presentation_block &block : blocks) {
    if (block.block == 0u) {
      continue;
    }

    lookup[block_position_key{block.x, block.y, block.z}] = block.block;
  }

  return lookup;
}

uint16_t find_block(const block_lookup &lookup, const block_position_key &key) {
  const auto iterator = lookup.find(key);
  return iterator == lookup.end() ? 0u : iterator->second;
}

bool has_block_override(const block_lookup &lookup, const block_position_key &key,
                        uint16_t &block) {
  const auto iterator = lookup.find(key);
  if (iterator == lookup.end()) {
    block = 0u;
    return false;
  }

  block = iterator->second;
  return true;
}

void apply_local_block_record(std::vector<presentation_block> &blocks,
                              const presentation_block &update) {
  for (auto iterator = blocks.begin(); iterator != blocks.end(); ++iterator) {
    if (iterator->x == update.x && iterator->y == update.y &&
        iterator->z == update.z) {
      if (update.block == 0u) {
        blocks.erase(iterator);
      } else {
        *iterator = update;
      }
      return;
    }
  }

  if (update.block != 0u) {
    blocks.push_back(update);
  }
}

int apply_snapshot_blocks(const std::vector<presentation_block> &blocks,
                          uint64_t tick_id) {
  std::vector<octaryn_replication_change> changes(blocks.size());
  for (size_t index = 0; index < blocks.size(); ++index) {
    const presentation_block &block = blocks[index];
    changes[index].version = 1u;
    changes[index].size = OCTARYN_REPLICATION_CHANGE_SIZE;
    changes[index].change_kind = 1u;
    changes[index].replication_id = static_cast<uint64_t>(index + 1u);
    changes[index].payload0 = pack_signed_pair(block.x, block.y);
    changes[index].payload1 = pack_block(block.z, block.block);
  }

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = static_cast<uint32_t>(changes.size());
  snapshot.tick_id = tick_id;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(changes.data()));

  return octaryn_client_apply_server_snapshot(&snapshot);
}

bool drain_presentation_updates(std::vector<presentation_block> &blocks,
                                uint32_t &written) {
  octaryn_replication_change changes[kMaxPresentationUpdatesPerFrame]{};
  written = 0u;
  const int result = octaryn_client_drain_presentation_updates(
      changes, kMaxPresentationUpdatesPerFrame, &written);
  if (result != 0) {
    log_result("drain_presentation_updates", result);
    return false;
  }

  for (uint32_t index = 0u; index < written; ++index) {
    apply_presentation_update(blocks, changes[index]);
  }

  if (written != 0u && g_log != nullptr) {
    std::fprintf(g_log, "presentation_updates_drained=%" PRIu32 "\n", written);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
