#pragma once

#include "BlockStore.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace octaryn::server::world::blocks {

struct BlockEditPolicy {
  std::function<uint16_t(const BlockPosition &position)> generated_block;
  std::function<bool(uint16_t block)> is_known_block;
  std::function<bool(const BlockEdit &edit, uint16_t below_block)>
      can_apply_edit;
  std::function<bool(uint16_t block, const BlockPosition &position,
                     uint16_t below_block)>
      can_stay_supported;
};

struct BlockEditApplyResult {
  BlockEditResult result;
  std::vector<BlockEdit> changes;
};

[[nodiscard]] uint16_t get_effective_block(const BlockStore &store,
                                           const BlockPosition &position,
                                           const BlockEditPolicy &policy);
[[nodiscard]] bool can_apply_block_edit(const BlockStore &store,
                                        const BlockEdit &edit,
                                        const BlockEditPolicy &policy);
BlockEditApplyResult apply_block_edit(BlockStore &store, const BlockEdit &edit,
                                      const BlockEditPolicy &policy);

} // namespace octaryn::server::world::blocks

extern "C" {

using octaryn_server_block_known_fn = uint32_t (*)(void *context,
                                                   uint16_t block);
using octaryn_server_block_can_apply_fn = uint32_t (*)(
    void *context, const octaryn_server_block_edit *edit, uint16_t below_block);
using octaryn_server_block_can_stay_supported_fn = uint32_t (*)(
    void *context, uint16_t block,
    const octaryn_server_block_position *position, uint16_t below_block);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_block_edit_service_can_apply(
    void *store, const octaryn_server_block_edit *edit,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit, void *context);

OCTARYN_SERVER_BLOCK_STORE_API octaryn_server_block_edit_result
octaryn_server_block_edit_service_apply(
    void *store, const octaryn_server_block_edit *edit,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_block_edit *changes, uint32_t change_capacity,
    uint32_t *change_count);
}
