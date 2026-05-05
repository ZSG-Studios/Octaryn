#include "BlockStore.h"

#include <algorithm>

namespace {

using octaryn::server::world::blocks::AirBlock;
using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockEditResult;
using octaryn::server::world::blocks::BlockPosition;
using octaryn::server::world::blocks::BlockStore;

BlockPosition to_block_position(const octaryn_server_block_position &position) {
  return BlockPosition{.x = position.x, .y = position.y, .z = position.z};
}

octaryn_server_block_position
to_abi_block_position(const BlockPosition &position) {
  return octaryn_server_block_position{
      .x = position.x,
      .y = position.y,
      .z = position.z,
  };
}

BlockEdit to_block_edit(const octaryn_server_block_edit &edit) {
  return BlockEdit{
      .position = to_block_position(edit.position),
      .block = edit.block,
  };
}

octaryn_server_block_edit to_abi_block_edit(const BlockEdit &edit) {
  return octaryn_server_block_edit{
      .position = to_abi_block_position(edit.position),
      .block = edit.block,
  };
}

octaryn_server_block_edit_result
to_abi_block_edit_result(const BlockEditResult &result) {
  return octaryn_server_block_edit_result{
      .applied = result.applied ? 1u : 0u,
      .changed = result.changed ? 1u : 0u,
      .edit = to_abi_block_edit(result.edit),
  };
}

BlockStore *as_store(void *store) { return static_cast<BlockStore *>(store); }

} // namespace

extern "C" {

void *octaryn_server_block_store_create() { return new BlockStore(); }

void octaryn_server_block_store_destroy(void *store) { delete as_store(store); }

uint64_t octaryn_server_block_store_block_count(void *store) {
  const auto *block_store = as_store(store);
  return block_store == nullptr ? 0u : block_store->block_count();
}

uint16_t octaryn_server_block_store_get_block(
    void *store, const octaryn_server_block_position *position) {
  const auto *block_store = as_store(store);
  if (block_store == nullptr || position == nullptr) {
    return AirBlock;
  }

  return block_store->get_block(to_block_position(*position));
}

uint32_t octaryn_server_block_store_try_get_block(
    void *store, const octaryn_server_block_position *position,
    uint16_t *block) {
  const auto *block_store = as_store(store);
  if (block != nullptr) {
    *block = AirBlock;
  }
  if (block_store == nullptr || position == nullptr || block == nullptr) {
    return 0u;
  }

  return block_store->try_get_block(to_block_position(*position), *block) ? 1u
                                                                         : 0u;
}

octaryn_server_block_edit_result
octaryn_server_block_store_clear_block_override(
    void *store, const octaryn_server_block_position *position) {
  auto *block_store = as_store(store);
  if (block_store == nullptr || position == nullptr) {
    return to_abi_block_edit_result({});
  }

  return to_abi_block_edit_result(
      block_store->clear_block_override(to_block_position(*position)));
}

octaryn_server_block_edit_result
octaryn_server_block_store_set_block(void *store,
                                     const octaryn_server_block_edit *edit,
                                     uint32_t preserve_air_override) {
  auto *block_store = as_store(store);
  if (block_store == nullptr || edit == nullptr) {
    return to_abi_block_edit_result({});
  }

  return to_abi_block_edit_result(
      block_store->set_block(to_block_edit(*edit),
                             preserve_air_override != 0u));
}

uint64_t octaryn_server_block_store_snapshot_count(void *store) {
  const auto *block_store = as_store(store);
  return block_store == nullptr ? 0u : block_store->snapshot().size();
}

uint64_t octaryn_server_block_store_snapshot_fill(
    void *store, octaryn_server_block_edit *edits, uint64_t capacity) {
  const auto *block_store = as_store(store);
  if (block_store == nullptr || (capacity > 0u && edits == nullptr)) {
    return 0u;
  }

  const auto snapshot = block_store->snapshot();
  const uint64_t written = std::min<uint64_t>(capacity, snapshot.size());
  for (uint64_t index = 0; index < written; ++index) {
    edits[index] = to_abi_block_edit(snapshot[index]);
  }
  return written;
}

uint64_t octaryn_server_block_store_snapshot_chunk_column_count(
    void *store, int32_t origin_x, int32_t origin_z) {
  const auto *block_store = as_store(store);
  return block_store == nullptr
             ? 0u
             : block_store->snapshot_chunk_column(origin_x, origin_z).size();
}

uint64_t octaryn_server_block_store_snapshot_chunk_column_fill(
    void *store, int32_t origin_x, int32_t origin_z,
    octaryn_server_block_edit *edits, uint64_t capacity) {
  const auto *block_store = as_store(store);
  if (block_store == nullptr || (capacity > 0u && edits == nullptr)) {
    return 0u;
  }

  const auto snapshot = block_store->snapshot_chunk_column(origin_x, origin_z);
  const uint64_t written = std::min<uint64_t>(capacity, snapshot.size());
  for (uint64_t index = 0; index < written; ++index) {
    edits[index] = to_abi_block_edit(snapshot[index]);
  }
  return written;
}

void octaryn_server_block_store_load(void *store,
                                     const octaryn_server_block_edit *edits,
                                     uint64_t count) {
  auto *block_store = as_store(store);
  if (block_store == nullptr || (count > 0u && edits == nullptr)) {
    return;
  }

  std::vector<BlockEdit> native_edits;
  native_edits.reserve(count);
  for (uint64_t index = 0; index < count; ++index) {
    native_edits.push_back(to_block_edit(edits[index]));
  }
  block_store->load(native_edits);
}

int32_t octaryn_server_block_store_clear_overrides_matching(
    void *store, octaryn_server_generated_block_fn generated_block,
    void *context) {
  auto *block_store = as_store(store);
  if (block_store == nullptr || generated_block == nullptr) {
    return 0;
  }

  return block_store->clear_overrides_matching(
      [generated_block, context](const BlockPosition &position) {
        const auto abi_position = to_abi_block_position(position);
        return generated_block(context, &abi_position);
      });
}

uint32_t octaryn_server_block_store_is_valid_position(
    const octaryn_server_block_position *position) {
  return position != nullptr &&
                 octaryn::server::world::blocks::is_valid_position(
                     to_block_position(*position))
             ? 1u
             : 0u;
}

octaryn_server_chunk_position octaryn_server_block_store_chunk_position_for(
    const octaryn_server_block_position *position) {
  if (position == nullptr) {
    return {};
  }

  const auto chunk = octaryn::server::world::blocks::chunk_position_for(
      to_block_position(*position));
  return octaryn_server_chunk_position{.x = chunk.x, .y = chunk.y, .z = chunk.z};
}

octaryn_server_block_position octaryn_server_block_store_local_position_for(
    const octaryn_server_block_position *position) {
  if (position == nullptr) {
    return {};
  }

  return to_abi_block_position(octaryn::server::world::blocks::local_position_for(
      to_block_position(*position)));
}

}
