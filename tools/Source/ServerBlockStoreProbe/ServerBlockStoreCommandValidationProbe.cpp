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
  return ok;
}
