#include "BlockCommandQueue.h"
#include "BlockStore.h"

#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using namespace octaryn::server::world::blocks;

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
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

struct DrainApplyObservation {
  int result_count = 0;
  int applied_count = 0;
  uint32_t changed_count = 0;
  uint32_t change_count = 0;
  octaryn_server_block_edit changes[2]{};
};

uint16_t generated_block(void *,
                         const octaryn_server_block_position *position) {
  return position != nullptr && position->y == 0 ? uint16_t{2u} : AirBlock;
}

uint32_t is_known_block(void *, uint16_t block) {
  return block == AirBlock || block == 2u || block == 5u ? 1u : 0u;
}

uint32_t can_apply_edit(void *, const octaryn_server_block_edit *edit,
                        uint16_t below_block) {
  return edit != nullptr && (edit->block != 5u || below_block != AirBlock) ? 1u
                                                                           : 0u;
}

uint32_t can_stay_supported(void *, uint16_t,
                            const octaryn_server_block_position *position,
                            uint16_t) {
  return position != nullptr ? 1u : 0u;
}

uint32_t is_client_placeable(void *, uint16_t block) {
  return block == 5u ? 1u : 0u;
}

uint32_t can_apply_command(void *, const octaryn_host_command *value) {
  return value != nullptr && is_valid_position(BlockPosition{
                                 .x = value->a, .y = value->b, .z = value->c})
             ? 1u
             : 0u;
}

uint32_t note_drain_apply_result(void *context, const octaryn_host_command *,
                                 const octaryn_server_block_edit_result *result,
                                 const octaryn_server_block_edit *changes,
                                 uint32_t change_count) {
  if (context == nullptr || result == nullptr) {
    return 0u;
  }

  auto *observation = static_cast<DrainApplyObservation *>(context);
  ++observation->result_count;
  if (result->applied != 0u) {
    ++observation->applied_count;
  }
  if (result->changed != 0u) {
    ++observation->changed_count;
  }
  observation->change_count = change_count;
  if (changes == nullptr) {
    return result->applied;
  }
  for (uint32_t index = 0u; index < change_count && index < 2u; ++index) {
    observation->changes[index] = changes[index];
  }
  return result->applied;
}

} // namespace

bool validate_command_queue() {
  ClientBlockCommandQueue queue;
  const BlockCommandQueuePolicy policy{
      .is_client_placeable = [](uint16_t block) { return block == 5u; },
      .can_apply =
          [](const octaryn_host_command &value) {
            return is_valid_position(
                BlockPosition{.x = value.a, .y = value.b, .z = value.c});
          }};

  bool ok = true;
  ok &= expect_equal("empty command queue count", queue.pending_count(),
                     size_t{0});
  ok &= expect_equal("native command queue capacity",
                     MaxPendingClientBlockCommands, size_t{4096});
  ok &= expect_equal("exported command queue capacity",
                     octaryn_server_client_block_command_queue_max_pending(),
                     uint64_t{MaxPendingClientBlockCommands});
  size_t rejected_index = 99u;

  std::vector<octaryn_host_command> oversized(MaxPendingClientBlockCommands +
                                              1u);
  ok &= expect_equal(
      "oversized command batch rejected",
      queue.submit(oversized.data(), oversized.size(), policy, rejected_index),
      -1);
  ok &= expect_equal("oversized batch leaves queue empty",
                     queue.pending_count(), size_t{0});

  octaryn_host_command invalid_version = command(0, 0, 0, 5);
  invalid_version.version = 0u;
  ok &= expect_equal("invalid version rejected",
                     queue.submit(&invalid_version, 1u, policy, rejected_index),
                     -2);
  ok &= expect_equal(
      "place command edit label",
      std::string_view{
          octaryn_server_client_block_command_edit_label(&invalid_version)},
      std::string_view{"place"});

  octaryn_host_command unsupported_kind = command(0, 0, 0, 5);
  unsupported_kind.kind = 99u;
  ok &= expect_equal(
      "unsupported kind rejected",
      queue.submit(&unsupported_kind, 1u, policy, rejected_index), -2);
  ok &= expect_equal(
      "unsupported command edit label",
      std::string_view{
          octaryn_server_client_block_command_edit_label(&unsupported_kind)},
      std::string_view{"none"});
  ok &= expect_equal("null command edit label",
                     std::string_view{
                         octaryn_server_client_block_command_edit_label(
                             nullptr)},
                     std::string_view{"none"});
  octaryn_host_command out_of_range_block = command(0, 0, 0, 70000);
  ok &= expect_equal(
      "out-of-range block rejected",
      queue.submit(&out_of_range_block, 1u, policy, rejected_index), -2);
  octaryn_host_command unplaceable_block = command(0, 0, 0, 7);
  ok &= expect_equal(
      "unplaceable block rejected",
      queue.submit(&unplaceable_block, 1u, policy, rejected_index), -2);
  octaryn_host_command out_of_bounds_command =
      command(0, WorldMaxYExclusive, 0, 5);
  ok &= expect_equal(
      "out-of-bounds regular command rejected",
      queue.submit(&out_of_bounds_command, 1u, policy, rejected_index), -2);
  ok &= expect_equal("rejected command queue count", queue.pending_count(),
                     size_t{0});

  octaryn_host_command rejected_batch[]{
      command(0, 0, 0, 5),
      command(0, WorldMaxYExclusive, 0, 5,
              OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG),
  };
  ok &= expect_equal("invalid batch rejected",
                     queue.submit(rejected_batch, 2u, policy, rejected_index),
                     -2);
  ok &= expect_equal("invalid batch rejected index", rejected_index, size_t{1});
  ok &= expect_equal("invalid batch leaves queue empty", queue.pending_count(),
                     size_t{0});

  octaryn_host_command accepted_batch[]{
      command(0, 0, 0, 5),
      command(0, 0, 0, 5, OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG),
  };
  octaryn_host_command break_command = command(0, 0, 0, AirBlock);
  ok &= expect_equal(
      "break command edit label",
      std::string_view{
          octaryn_server_client_block_command_edit_label(&break_command)},
      std::string_view{"break"});
  ClientBlockCommandQueue report_queue;
  octaryn_server_client_block_command_submit_report submit_report{};
  ok &= expect_equal("submit report accepted result",
                     octaryn_server_client_block_command_queue_submit_report(
                         &report_queue, accepted_batch, 2u, is_client_placeable,
                         can_apply_command, nullptr, &submit_report),
                     0);
  ok &= expect_equal("submit report accepted reason", submit_report.reason,
                     uint32_t{0u});
  ok &= expect_equal("submit report requested count",
                     submit_report.requested_count, uint32_t{2u});
  ok &= expect_equal("submit report pending before",
                     submit_report.pending_before, uint64_t{0u});
  ok &= expect_equal("submit report pending after", submit_report.pending_after,
                     uint64_t{2u});
  report_queue.drain([](const octaryn_host_command &) { return true; });

  ok &= expect_equal("submit report missing buffer result",
                     octaryn_server_client_block_command_queue_submit_report(
                         &report_queue, nullptr, 1u, is_client_placeable,
                         can_apply_command, nullptr, &submit_report),
                     -1);
  ok &= expect_equal("submit report capacity reason", submit_report.reason,
                     uint32_t{1u});

  ok &=
      expect_equal("valid batch submit",
                   queue.submit(accepted_batch, 2u, policy, rejected_index), 0);
  ok &= expect_equal("queued command count", queue.pending_count(), size_t{2});

  int observed = 0;
  const int applied =
      queue.drain([&observed](const octaryn_host_command &value) {
        ++observed;
        return is_valid_position(
            BlockPosition{.x = value.a, .y = value.b, .z = value.c});
      });

  ok &= expect_equal("drain observed command count", observed, 2);
  ok &= expect_equal("drain applied command count", applied, 2);
  ok &= expect_equal("drain clears command queue", queue.pending_count(),
                     size_t{0});

  ClientBlockCommandQueue apply_queue;
  BlockStore store;
  octaryn_host_command applied_command = command(0, 1, 0, 5);
  ok &= expect_equal(
      "drain apply submit",
      apply_queue.submit(&applied_command, 1u, policy, rejected_index), 0);
  DrainApplyObservation observation{};
  ok &= expect_equal("drain apply result",
                     octaryn_server_client_block_command_queue_drain_apply(
                         &apply_queue, &store, generated_block, is_known_block,
                         can_apply_edit, can_stay_supported, nullptr,
                         note_drain_apply_result, &observation),
                     1);
  ok &= expect_equal("drain apply callback count", observation.result_count, 1);
  ok &= expect_equal("drain apply applied count", observation.applied_count, 1);
  ok &= expect_equal("drain apply changed count", observation.changed_count,
                     uint32_t{1u});
  ok &= expect_equal("drain apply change count", observation.change_count,
                     uint32_t{1u});
  ok &= expect_equal("drain apply changed block", observation.changes[0].block,
                     uint16_t{5u});
  ok &= expect_equal("drain apply store block",
                     store.get_block(BlockPosition{.x = 0, .y = 1, .z = 0}),
                     uint16_t{5u});
  ok &= expect_equal("drain apply clears command queue",
                     apply_queue.pending_count(), size_t{0});

  ClientBlockCommandQueue rejected_apply_queue;
  BlockStore rejected_store;
  octaryn_host_command unsupported_command = command(0, 2, 0, 5);
  ok &= expect_equal("drain apply unsupported submit",
                     rejected_apply_queue.submit(&unsupported_command, 1u,
                                                 policy, rejected_index),
                     0);
  DrainApplyObservation rejected_observation{};
  ok &=
      expect_equal("drain apply unsupported result",
                   octaryn_server_client_block_command_queue_drain_apply(
                       &rejected_apply_queue, &rejected_store, generated_block,
                       is_known_block, can_apply_edit, can_stay_supported,
                       nullptr, note_drain_apply_result, &rejected_observation),
                   0);
  ok &= expect_equal("drain apply unsupported callback count",
                     rejected_observation.result_count, 1);
  ok &= expect_equal("drain apply unsupported applied count",
                     rejected_observation.applied_count, 0);
  ok &= expect_equal("drain apply unsupported changed count",
                     rejected_observation.changed_count, uint32_t{0u});
  ok &= expect_equal("drain apply unsupported clears command queue",
                     rejected_apply_queue.pending_count(), size_t{0});
  return ok;
}
