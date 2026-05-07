#include "BlockEditService.h"
#include "BlockChangeQueue.h"
#include "BlockCommandQueue.h"

namespace octaryn::server::world::blocks {
namespace {

BlockEditResult invalid_edit() {
  return BlockEditResult{.applied = false, .changed = false, .edit = {}};
}

BlockEditResult changed_edit(const BlockEdit &edit) {
  return BlockEditResult{.applied = true, .changed = true, .edit = edit};
}

BlockEditResult apply_override(BlockStore &store, const BlockEdit &edit,
                               const BlockEditPolicy &policy) {
  const uint16_t generated_block =
      policy.generated_block ? policy.generated_block(edit.position) : AirBlock;
  uint16_t existing_override = AirBlock;
  const bool has_override =
      store.try_get_block(edit.position, existing_override);
  const uint16_t current_block =
      has_override ? existing_override : generated_block;
  if (current_block == edit.block) {
    return BlockEditResult{.applied = true, .changed = false, .edit = {}};
  }

  if (has_override && edit.block == generated_block) {
    const BlockEditResult cleared = store.clear_block_override(edit.position);
    return cleared.changed ? changed_edit(edit) : cleared;
  }

  return store.set_block(edit,
                         edit.block == AirBlock && generated_block != AirBlock);
}

BlockEdit block_edit_from_command(const octaryn_host_command &command) {
  return BlockEdit{
      .position = BlockPosition{.x = command.a, .y = command.b, .z = command.c},
      .block = host_command_block(command),
  };
}

} // namespace

BlockEditPolicy
policy_from_abi(octaryn_server_generated_block_fn generated_block,
                octaryn_server_block_known_fn is_known_block,
                octaryn_server_block_can_apply_fn can_apply_edit,
                octaryn_server_block_can_stay_supported_fn can_stay_supported,
                void *context) {
  return BlockEditPolicy{
      .generated_block =
          [generated_block, context](const BlockPosition &position) {
            if (generated_block == nullptr) {
              return AirBlock;
            }

            const octaryn_server_block_position native_position{
                .x = position.x, .y = position.y, .z = position.z};
            return generated_block(context, &native_position);
          },
      .is_known_block =
          [is_known_block, context](uint16_t block) {
            return is_known_block != nullptr &&
                   is_known_block(context, block) != 0u;
          },
      .can_apply_edit =
          [can_apply_edit, context](const BlockEdit &edit,
                                    uint16_t below_block) {
            if (can_apply_edit == nullptr) {
              return false;
            }

            const octaryn_server_block_edit native_edit{
                .position = octaryn_server_block_position{.x = edit.position.x,
                                                          .y = edit.position.y,
                                                          .z = edit.position.z},
                .block = edit.block};
            return can_apply_edit(context, &native_edit, below_block) != 0u;
          },
      .can_stay_supported =
          [can_stay_supported, context](uint16_t block,
                                        const BlockPosition &position,
                                        uint16_t below_block) {
            if (can_stay_supported == nullptr) {
              return false;
            }

            const octaryn_server_block_position native_position{
                .x = position.x, .y = position.y, .z = position.z};
            return can_stay_supported(context, block, &native_position,
                                      below_block) != 0u;
          }};
}

octaryn_server_block_edit to_abi_block_edit(const BlockEdit &edit) {
  return octaryn_server_block_edit{
      .position = octaryn_server_block_position{.x = edit.position.x,
                                                .y = edit.position.y,
                                                .z = edit.position.z},
      .block = edit.block};
}

octaryn_server_block_edit_result to_abi_result(const BlockEditResult &result) {
  return octaryn_server_block_edit_result{.applied = result.applied ? 1u : 0u,
                                          .changed = result.changed ? 1u : 0u,
                                          .edit =
                                              to_abi_block_edit(result.edit)};
}

uint16_t get_effective_block(const BlockStore &store,
                             const BlockPosition &position,
                             const BlockEditPolicy &policy) {
  uint16_t block = AirBlock;
  if (store.try_get_block(position, block)) {
    return block;
  }

  return policy.generated_block ? policy.generated_block(position) : AirBlock;
}

bool can_apply_block_edit(const BlockStore &store, const BlockEdit &edit,
                          const BlockEditPolicy &policy) {
  if (!is_valid_position(edit.position) || !policy.is_known_block ||
      !policy.is_known_block(edit.block)) {
    return false;
  }

  if (edit.block == AirBlock) {
    return true;
  }

  const BlockPosition below_position{
      .x = edit.position.x, .y = edit.position.y - 1, .z = edit.position.z};
  return policy.can_apply_edit &&
         policy.can_apply_edit(
             edit, get_effective_block(store, below_position, policy));
}

bool can_apply_block_command(const BlockStore &store,
                             const octaryn_host_command &command,
                             const BlockEditPolicy &policy) {
  if (!host_command_is_supported_set_block(command)) {
    return false;
  }

  const auto native_edit = block_edit_from_command(command);
  if (host_command_is_client_interaction(command)) {
    const auto hit_position = host_command_interaction_hit_position(command);
    const uint16_t hit_block =
        get_effective_block(store, hit_position, policy);
    const uint16_t edit_position_block =
        get_effective_block(store, native_edit.position, policy);
    if (!host_command_client_interaction_is_valid(command, hit_block,
                                                  edit_position_block)) {
      return false;
    }
  }

  return can_apply_block_edit(store, native_edit, policy);
}

BlockEditApplyResult apply_block_edit(BlockStore &store, const BlockEdit &edit,
                                      const BlockEditPolicy &policy) {
  if (!can_apply_block_edit(store, edit, policy)) {
    return BlockEditApplyResult{.result = invalid_edit(), .changes = {}};
  }

  const BlockEditResult result = apply_override(store, edit, policy);
  if (!result.changed || edit.position.y + 1 >= WorldMaxYExclusive) {
    return BlockEditApplyResult{
        .result = result,
        .changes = result.changed ? std::vector<BlockEdit>{result.edit}
                                  : std::vector<BlockEdit>{}};
  }

  const BlockPosition above_position{
      .x = edit.position.x, .y = edit.position.y + 1, .z = edit.position.z};
  const uint16_t above_block =
      get_effective_block(store, above_position, policy);
  if (above_block == AirBlock ||
      (policy.can_stay_supported &&
       policy.can_stay_supported(above_block, above_position, edit.block))) {
    return BlockEditApplyResult{.result = result,
                                .changes = std::vector<BlockEdit>{result.edit}};
  }

  const BlockEdit cascade_edit{.position = above_position, .block = AirBlock};
  const BlockEditResult cascade_result =
      apply_override(store, cascade_edit, policy);
  if (!cascade_result.changed) {
    return BlockEditApplyResult{.result = result,
                                .changes = std::vector<BlockEdit>{result.edit}};
  }

  return BlockEditApplyResult{
      .result = BlockEditResult{.applied = true, .changed = true, .edit = edit},
      .changes = std::vector<BlockEdit>{result.edit, cascade_result.edit}};
}

BlockEditApplyResult apply_block_command(BlockStore &store,
                                         const octaryn_host_command &command,
                                         const BlockEditPolicy &policy) {
  if (!can_apply_block_command(store, command, policy)) {
    return BlockEditApplyResult{.result = invalid_edit(), .changes = {}};
  }

  const auto native_edit = block_edit_from_command(command);
  return apply_block_edit(store, native_edit, policy);
}

} // namespace octaryn::server::world::blocks

extern "C" {

uint32_t octaryn_server_block_edit_service_can_apply(
    void *store, const octaryn_server_block_edit *edit,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit, void *context) {
  if (store == nullptr || edit == nullptr) {
    return 0u;
  }

  const auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  const octaryn::server::world::blocks::BlockEdit native_edit{
      .position =
          octaryn::server::world::blocks::BlockPosition{.x = edit->position.x,
                                                        .y = edit->position.y,
                                                        .z = edit->position.z},
      .block = edit->block};
  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, nullptr, context);
  return octaryn::server::world::blocks::can_apply_block_edit(
             *block_store, native_edit, policy)
             ? 1u
             : 0u;
}

uint32_t octaryn_server_block_edit_service_can_apply_command(
    void *store, const octaryn_host_command *command,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit, void *context) {
  if (store == nullptr || command == nullptr ||
      !octaryn::server::world::blocks::host_command_is_supported_set_block(
          *command)) {
    return 0u;
  }

  const auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, nullptr, context);
  return octaryn::server::world::blocks::can_apply_block_command(
             *block_store, *command, policy)
             ? 1u
             : 0u;
}

octaryn_server_block_edit_result octaryn_server_block_edit_service_apply(
    void *store, const octaryn_server_block_edit *edit,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_block_edit *changes, uint32_t change_capacity,
    uint32_t *change_count) {
  if (change_count != nullptr) {
    *change_count = 0u;
  }

  if (store == nullptr || edit == nullptr) {
    return octaryn::server::world::blocks::to_abi_result(
        octaryn::server::world::blocks::BlockEditResult{
            .applied = false, .changed = false, .edit = {}});
  }

  auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  const octaryn::server::world::blocks::BlockEdit native_edit{
      .position =
          octaryn::server::world::blocks::BlockPosition{.x = edit->position.x,
                                                        .y = edit->position.y,
                                                        .z = edit->position.z},
      .block = edit->block};
  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, can_stay_supported,
      context);
  const auto result = octaryn::server::world::blocks::apply_block_edit(
      *block_store, native_edit, policy);
  if (change_count != nullptr) {
    *change_count = static_cast<uint32_t>(result.changes.size());
  }

  if (changes != nullptr && change_capacity >= result.changes.size()) {
    for (size_t index = 0; index < result.changes.size(); ++index) {
      changes[index] = octaryn::server::world::blocks::to_abi_block_edit(
          result.changes[index]);
    }
  }

  return octaryn::server::world::blocks::to_abi_result(result.result);
}

octaryn_server_block_edit_result
octaryn_server_block_edit_service_apply_command(
    void *store, const octaryn_host_command *command,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_block_edit *changes, uint32_t change_capacity,
    uint32_t *change_count) {
  if (change_count != nullptr) {
    *change_count = 0u;
  }

  if (store == nullptr || command == nullptr ||
      !octaryn::server::world::blocks::host_command_is_supported_set_block(
          *command)) {
    return octaryn::server::world::blocks::to_abi_result(
        octaryn::server::world::blocks::BlockEditResult{
            .applied = false, .changed = false, .edit = {}});
  }

  auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, can_stay_supported,
      context);
  const auto result = octaryn::server::world::blocks::apply_block_command(
      *block_store, *command, policy);
  if (change_count != nullptr) {
    *change_count = static_cast<uint32_t>(result.changes.size());
  }

  if (changes != nullptr && change_capacity >= result.changes.size()) {
    for (size_t index = 0; index < result.changes.size(); ++index) {
      changes[index] = octaryn::server::world::blocks::to_abi_block_edit(
          result.changes[index]);
    }
  }

  return octaryn::server::world::blocks::to_abi_result(result.result);
}

octaryn_server_block_edit_result
octaryn_server_block_edit_service_apply_command_and_enqueue(
    void *store, void *change_queue, const octaryn_host_command *command,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_block_edit *changes, uint32_t change_capacity,
    uint32_t *change_count) {
  if (change_count != nullptr) {
    *change_count = 0u;
  }

  if (store == nullptr || command == nullptr ||
      !octaryn::server::world::blocks::host_command_is_supported_set_block(
          *command)) {
    return octaryn::server::world::blocks::to_abi_result(
        octaryn::server::world::blocks::BlockEditResult{
            .applied = false, .changed = false, .edit = {}});
  }

  auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  auto *block_changes =
      static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(
          change_queue);
  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, can_stay_supported,
      context);
  const auto result = octaryn::server::world::blocks::apply_block_command(
      *block_store, *command, policy);
  if (block_changes != nullptr) {
    block_changes->enqueue_all(result.changes);
  }
  if (change_count != nullptr) {
    *change_count = static_cast<uint32_t>(result.changes.size());
  }

  if (changes != nullptr && change_capacity >= result.changes.size()) {
    for (size_t index = 0; index < result.changes.size(); ++index) {
      changes[index] = octaryn::server::world::blocks::to_abi_block_edit(
          result.changes[index]);
    }
  }

  return octaryn::server::world::blocks::to_abi_result(result.result);
}
}
