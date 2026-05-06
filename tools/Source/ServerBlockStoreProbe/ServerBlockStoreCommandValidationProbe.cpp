#include "BlockCommandQueue.h"
#include "BlockEditService.h"
#include "BlockStore.h"

#include <cstdio>
#include <string_view>

namespace {

using namespace octaryn::server::world::blocks;

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

octaryn_host_command command(int32_t x, int32_t y, int32_t z, int32_t block,
                             uint32_t flags = 0u) {
  octaryn_host_command value{};
  value.version = HostCommandVersion;
  value.size = OCTARYN_HOST_COMMAND_SIZE;
  value.kind = HostCommandSetBlockKind;
  value.flags = flags;
  value.a = x;
  value.b = y;
  value.c = z;
  value.d = block;
  return value;
}

uint16_t generated_block(void *,
                         const octaryn_server_block_position *position) {
  return position != nullptr && position->y == 0 ? uint16_t{2} : AirBlock;
}

uint32_t is_known_block(void *, uint16_t block) {
  return block == AirBlock || block == 2u || block == 5u || block == 9u ? 1u
                                                                        : 0u;
}

uint32_t can_apply_edit(void *, const octaryn_server_block_edit *edit,
                        uint16_t below_block) {
  return edit != nullptr && (edit->block != 9u || below_block != AirBlock) ? 1u
                                                                           : 0u;
}

uint32_t can_stay_supported(void *, uint16_t block,
                            const octaryn_server_block_position *position,
                            uint16_t below_block) {
  return position != nullptr && (block != 9u || below_block != AirBlock) ? 1u
                                                                         : 0u;
}

bool can_apply(BlockStore &store, octaryn_host_command &value) {
  return octaryn_server_block_edit_service_can_apply_command(
             &store, &value, generated_block, is_known_block, can_apply_edit,
             nullptr) != 0u;
}

} // namespace

bool validate_block_command_validation() {
  BlockStore store;
  bool ok = true;

  octaryn_host_command supported_block = command(0, 1, 0, 5);
  octaryn_server_block_edit changes[2]{};
  uint32_t change_count = 0u;

  ok &= expect_true("command validation supported block accepted",
                    can_apply(store, supported_block));
  octaryn_host_command unknown_block = command(0, 1, 0, 99);
  ok &= expect_true("command validation unknown block rejected",
                    !can_apply(store, unknown_block));
  octaryn_host_command unsupported_dependent_block = command(0, 2, 0, 9);
  ok &= expect_true("command validation unsupported dependent block rejected",
                    !can_apply(store, unsupported_dependent_block));

  octaryn_host_command adjacent_place =
      command(0, 1, 0, 5, OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG);
  adjacent_place.x = 0.5f;
  adjacent_place.y = 0.5f;
  adjacent_place.z = 0.5f;
  adjacent_place.x2 = 0.0f;
  adjacent_place.y2 = 0.0f;
  adjacent_place.z2 = 0.0f;
  ok &= expect_true("command validation generated-hit interaction accepted",
                    can_apply(store, adjacent_place));

  octaryn_host_command occupied_place =
      command(1, 0, 0, 5, OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG);
  occupied_place.x = 0.5f;
  occupied_place.y = 0.5f;
  occupied_place.z = 0.5f;
  occupied_place.x2 = 0.0f;
  occupied_place.y2 = 0.0f;
  occupied_place.z2 = 0.0f;
  ok &= expect_true("command validation occupied interaction rejected",
                    !can_apply(store, occupied_place));

  auto occupied_result = octaryn_server_block_edit_service_apply_command(
      &store, &occupied_place, generated_block, is_known_block, can_apply_edit,
      can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply rejects occupied interaction",
                    occupied_result.applied == 0u && change_count == 0u);

  auto result = octaryn_server_block_edit_service_apply_command(
      &store, &supported_block, generated_block, is_known_block,
      can_apply_edit, can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply accepted", result.applied != 0u);
  ok &= expect_true("command apply changed", result.changed != 0u);
  ok &= expect_true("command apply change count", change_count == 1u);
  ok &= expect_true("command apply edit x", changes[0].position.x == 0);
  ok &= expect_true("command apply edit block", changes[0].block == 5u);

  result = octaryn_server_block_edit_service_apply_command(
      &store, &unknown_block, generated_block, is_known_block, can_apply_edit,
      can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply rejects unknown block",
                    result.applied == 0u && change_count == 0u);

  octaryn_host_command support_block = command(2, 0, 0, 5);
  result = octaryn_server_block_edit_service_apply_command(
      &store, &support_block, generated_block, is_known_block, can_apply_edit,
      can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply support block changed",
                    result.applied != 0u && result.changed != 0u &&
                        change_count == 1u);

  octaryn_host_command dependent_block = command(2, 1, 0, 9);
  result = octaryn_server_block_edit_service_apply_command(
      &store, &dependent_block, generated_block, is_known_block,
      can_apply_edit, can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply dependent block changed",
                    result.applied != 0u && result.changed != 0u &&
                        change_count == 1u);

  octaryn_host_command remove_support = command(2, 0, 0, AirBlock);
  result = octaryn_server_block_edit_service_apply_command(
      &store, &remove_support, generated_block, is_known_block, can_apply_edit,
      can_stay_supported, nullptr, changes, 2u, &change_count);
  ok &= expect_true("command apply cascades unsupported block",
                    result.applied != 0u && result.changed != 0u &&
                        change_count == 2u);
  ok &= expect_true("command apply cascade clears dependent",
                    changes[1].position.x == 2 && changes[1].position.y == 1 &&
                        changes[1].position.z == 0 &&
                        changes[1].block == AirBlock);
  return ok;
}
