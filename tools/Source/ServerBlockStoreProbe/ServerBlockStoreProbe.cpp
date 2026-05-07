#include "BlockChangeQueue.h"
#include "BlockCommandQueue.h"
#include "BlockEditService.h"
#include "BlockStore.h"

#include <cstdio>
#include <limits>
#include <string_view>

bool validate_chunk_stream();
bool validate_block_command_validation();
bool validate_command_queue();
bool validate_chunk_stream_process_tick();

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

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

BlockEdit edit(int32_t x, int32_t y, int32_t z, uint16_t block) {
  return BlockEdit{.position = BlockPosition{.x = x, .y = y, .z = z},
                   .block = block};
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

bool validate_positions() {
  bool ok = true;
  ok &= expect_true("min y valid", is_valid_position(BlockPosition{
                                       .x = 0, .y = WorldMinY, .z = 0}));
  ok &= expect_true("max y invalid",
                    !is_valid_position(BlockPosition{
                        .x = 0, .y = WorldMaxYExclusive, .z = 0}));

  const ChunkPosition negative_chunk =
      chunk_position_for(BlockPosition{.x = -33, .y = -1, .z = -32});
  const BlockPosition negative_local =
      local_position_for(BlockPosition{.x = -33, .y = -1, .z = -32});

  ok &= expect_equal("negative chunk x", negative_chunk.x, -2);
  ok &= expect_equal("negative chunk y", negative_chunk.y, -1);
  ok &= expect_equal("negative chunk z", negative_chunk.z, -1);
  ok &= expect_equal("negative local x", negative_local.x, 31);
  ok &= expect_equal("negative local y", negative_local.y, 31);
  ok &= expect_equal("negative local z", negative_local.z, 0);
  return ok;
}

bool validate_edits_and_air_overrides() {
  BlockStore store;
  bool ok = true;

  ok &= expect_equal("empty count", store.block_count(), size_t{0});
  ok &= expect_equal("empty get", store.get_block({.x = 0, .y = 0, .z = 0}),
                     AirBlock);

  const auto set_stone = store.set_block(edit(1, 0, 2, 7));
  ok &= expect_true("set stone applied", set_stone.applied);
  ok &= expect_true("set stone changed", set_stone.changed);
  ok &= expect_equal("stone count", store.block_count(), size_t{1});
  ok &= expect_equal("stone read", store.get_block({.x = 1, .y = 0, .z = 2}),
                     uint16_t{7});

  const auto repeat_stone = store.set_block(edit(1, 0, 2, 7));
  ok &= expect_true("repeat stone applied", repeat_stone.applied);
  ok &= expect_true("repeat stone unchanged", !repeat_stone.changed);

  const auto air_without_preserve = store.set_block(edit(1, 0, 2, AirBlock));
  ok &= expect_true("air remove changed", air_without_preserve.changed);
  ok &= expect_equal("air remove count", store.block_count(), size_t{0});

  const auto preserved_air =
      store.set_block(edit(-32, -256, -1, AirBlock), true);
  ok &= expect_true("preserved air changed", preserved_air.changed);
  ok &= expect_equal("preserved air count", store.block_count(), size_t{1});
  uint16_t preserved_air_block = 99;
  ok &= expect_true(
      "preserved air try get",
      store.try_get_block({.x = -32, .y = -256, .z = -1}, preserved_air_block));
  ok &= expect_equal("preserved air block", preserved_air_block, AirBlock);

  const auto clear_air =
      store.clear_block_override({.x = -32, .y = -256, .z = -1});
  ok &= expect_true("clear preserved air changed", clear_air.changed);
  ok &=
      expect_equal("clear preserved air count", store.block_count(), size_t{0});

  const auto invalid_set = store.set_block(edit(0, WorldMaxYExclusive, 0, 5));
  ok &= expect_true("invalid set rejected", !invalid_set.applied);
  return ok;
}

bool validate_snapshot_and_load() {
  BlockStore store;
  store.set_block(edit(33, 0, 0, 2));
  store.set_block(edit(-1, -1, -1, 3));
  store.set_block(edit(0, -256, 0, 4));

  const std::vector<BlockEdit> snapshot = store.snapshot();
  bool ok = true;
  ok &= expect_equal("snapshot count", snapshot.size(), size_t{3});
  ok &= expect_equal("snapshot first block", snapshot[0].block, uint16_t{3});
  ok &= expect_equal("snapshot second block", snapshot[1].block, uint16_t{4});
  ok &= expect_equal("snapshot third block", snapshot[2].block, uint16_t{2});

  const std::vector<BlockEdit> column = store.snapshot_chunk_column(32, 0);
  ok &= expect_equal("column count", column.size(), size_t{1});
  ok &= expect_equal("column block", column[0].block, uint16_t{2});

  BlockStore loaded;
  loaded.load({edit(0, 0, 0, AirBlock), edit(0, 1, 0, 9)});
  ok &= expect_equal("loaded count", loaded.block_count(), size_t{2});
  ok &= expect_equal("loaded air override",
                     loaded.get_block({.x = 0, .y = 0, .z = 0}), AirBlock);
  ok &= expect_equal("loaded non-air",
                     loaded.get_block({.x = 0, .y = 1, .z = 0}), uint16_t{9});
  return ok;
}

bool validate_clear_generated_matches() {
  BlockStore store;
  store.set_block(edit(0, 0, 0, 1));
  store.set_block(edit(0, 1, 0, 2));
  store.set_block(edit(0, 2, 0, 3));

  const int32_t cleared = store.clear_overrides_matching(
      [](const BlockPosition &position) -> uint16_t {
        return position.y == 1 ? 2 : AirBlock;
      });

  bool ok = true;
  ok &= expect_equal("generated clear count", cleared, 1);
  ok &=
      expect_equal("generated remaining count", store.block_count(), size_t{2});
  ok &= expect_equal("generated cleared block",
                     store.get_block({.x = 0, .y = 1, .z = 0}), AirBlock);
  return ok;
}

BlockEditPolicy block_edit_policy() {
  return BlockEditPolicy{
      .generated_block = [](const BlockPosition &position) -> uint16_t {
        return position.y == 0 ? uint16_t{2} : AirBlock;
      },
      .is_known_block =
          [](uint16_t block) {
            return block == AirBlock || block == 2u || block == 5u ||
                   block == 9u;
          },
      .can_apply_edit =
          [](const BlockEdit &edit, uint16_t below_block) {
            return edit.block != 9u || below_block != AirBlock;
          },
      .can_stay_supported =
          [](uint16_t block, const BlockPosition &, uint16_t below_block) {
            return block != 9u || below_block != AirBlock;
          }};
}

bool validate_block_edit_service() {
  BlockStore store;
  const BlockEditPolicy policy = block_edit_policy();
  bool ok = true;

  ok &= expect_true("service unknown block rejected",
                    !can_apply_block_edit(store, edit(0, 0, 0, 99), policy));
  ok &= expect_true("service unsupported block rejected",
                    !can_apply_block_edit(store, edit(0, 2, 0, 9), policy));
  ok &= expect_true("service supported block accepted",
                    can_apply_block_edit(store, edit(0, 1, 0, 9), policy));

  const auto unchanged = apply_block_edit(store, edit(0, 0, 0, 2), policy);
  ok &= expect_true("service generated unchanged applied",
                    unchanged.result.applied);
  ok &= expect_true("service generated unchanged", !unchanged.result.changed);
  ok &= expect_equal("service generated unchanged changes",
                     unchanged.changes.size(), size_t{0});

  const auto preserved_air =
      apply_block_edit(store, edit(0, 0, 0, AirBlock), policy);
  ok &= expect_true("service preserved air changed",
                    preserved_air.result.changed);
  ok &= expect_equal("service preserved air count", store.block_count(),
                     size_t{1});
  uint16_t override_block = 7u;
  ok &= expect_true(
      "service preserved air override",
      store.try_get_block({.x = 0, .y = 0, .z = 0}, override_block));
  ok &= expect_equal("service preserved air block", override_block, AirBlock);

  const auto cleared_generated =
      apply_block_edit(store, edit(0, 0, 0, 2), policy);
  ok &= expect_true("service generated clear changed",
                    cleared_generated.result.changed);
  ok &= expect_equal("service generated clear count", store.block_count(),
                     size_t{0});

  const auto placed_support = apply_block_edit(store, edit(1, 0, 0, 5), policy);
  ok &= expect_true("service support place changed",
                    placed_support.result.changed);
  const auto placed_dependent =
      apply_block_edit(store, edit(1, 1, 0, 9), policy);
  ok &= expect_true("service dependent place changed",
                    placed_dependent.result.changed);
  const auto removed_support =
      apply_block_edit(store, edit(1, 0, 0, AirBlock), policy);
  ok &= expect_true("service cascade changed", removed_support.result.changed);
  ok &= expect_equal("service cascade count", removed_support.changes.size(),
                     size_t{2});
  ok &= expect_equal("service cascaded block cleared",
                     store.get_block({.x = 1, .y = 1, .z = 0}), AirBlock);
  return ok;
}

int32_t unpack_low(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int32_t unpack_high(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value >> 32u));
}

bool validate_change_queue() {
  BlockChangeQueue queue;
  bool ok = true;
  ok &= expect_equal("empty queue count", queue.pending_count(), size_t{0});

  queue.enqueue(edit(-4, 5, -6, 7));
  queue.enqueue(edit(8, -9, 10, AirBlock));
  ok &= expect_equal("queued change count", queue.pending_count(), size_t{2});

  ReplicationChange too_small[1]{};
  uint32_t written = 99u;
  ok &= expect_equal("drain too small",
                     queue.drain(too_small, 1u, 42u, written), -1);
  ok &= expect_equal("drain too small written", written, 0u);
  ok &= expect_equal("drain too small keeps queue", queue.pending_count(),
                     size_t{2});

  ReplicationChange changes[2]{};
  ok &= expect_equal("drain result", queue.drain(changes, 2u, 42u, written), 0);
  ok &= expect_equal("drain written", written, 2u);
  ok &= expect_equal("drain queue empty", queue.pending_count(), size_t{0});
  ok &= expect_equal("change version", changes[0].version,
                     ReplicationChangeVersion);
  ok &= expect_equal("change size", changes[0].size, ReplicationChangeSize);
  ok &=
      expect_equal("change kind", changes[0].change_kind, BlockEditChangeKind);
  ok &= expect_equal("change tick", changes[0].replication_id, uint64_t{42});
  ok &= expect_equal("change x", unpack_low(changes[0].payload0), -4);
  ok &= expect_equal("change y", unpack_high(changes[0].payload0), 5);
  ok &= expect_equal("change z", unpack_low(changes[0].payload1), -6);
  ok &= expect_equal("change block",
                     static_cast<uint16_t>(changes[0].payload1 >> 32u),
                     uint16_t{7});
  ok &=
      expect_equal("second change block",
                   static_cast<uint16_t>(changes[1].payload1 >> 32u), AirBlock);

  ok &= expect_equal("empty drain", queue.drain(changes, 0u, 43u, written), 0);
  ok &= expect_equal("empty drain written", written, 0u);

  BlockChangeQueue snapshot_queue;
  snapshot_queue.enqueue(edit(1, 2, 3, 4));
  ReplicationChange snapshot_changes[1]{};
  octaryn_server_snapshot_header header{};
  header.version = 1u;
  header.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  header.change_count = 1u;
  header.replication_ids_address = 123u;
  header.changes_address = reinterpret_cast<uint64_t>(snapshot_changes);
  uint64_t pending_before = 0u;
  ok &= expect_equal(
      "snapshot drain result",
      octaryn_server_block_change_queue_drain_snapshot(
          &snapshot_queue, &header, 44u, &pending_before, &written),
      0);
  ok &= expect_equal("snapshot drain pending", pending_before, uint64_t{1u});
  ok &= expect_equal("snapshot drain written", written, 1u);
  ok &= expect_equal("snapshot header change count", header.change_count, 1u);
  ok &= expect_equal("snapshot header tick", header.tick_id, uint64_t{44u});
  ok &= expect_equal("snapshot change block",
                     static_cast<uint16_t>(snapshot_changes[0].payload1 >> 32u),
                     uint16_t{4u});

  BlockChangeQueue report_queue;
  report_queue.enqueue(edit(7, 8, 9, 10));
  ReplicationChange report_changes[1]{};
  octaryn_server_snapshot_header report_header{};
  report_header.version = 1u;
  report_header.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  report_header.change_count = 1u;
  report_header.changes_address = reinterpret_cast<uint64_t>(report_changes);
  octaryn_server_block_change_snapshot_drain_report report{};
  ok &= expect_equal(
      "snapshot drain report result",
      octaryn_server_block_change_queue_drain_snapshot_report(
          &report_queue, &report_header, 45u, &report),
      0);
  ok &= expect_equal("snapshot drain report requested capacity",
                     report.requested_capacity, uint64_t{1u});
  ok &= expect_equal("snapshot drain report pending", report.pending_before,
                     uint64_t{1u});
  ok &= expect_equal("snapshot drain report written", report.written,
                     uint32_t{1u});
  ok &= expect_equal("snapshot drain report stored result", report.result, 0);
  ok &= expect_equal("snapshot drain report header tick", report_header.tick_id,
                     uint64_t{45u});
  return ok;
}

bool validate_client_interaction_policy() {
  octaryn_host_command place =
      command(1, 0, 0, 5, OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG);
  place.x = 0.5f;
  place.y = 0.5f;
  place.z = 0.5f;
  place.x2 = 0.0f;
  place.y2 = 0.0f;
  place.z2 = 0.0f;

  octaryn_server_block_position hit_position{};
  bool ok = true;
  ok &= expect_true("interaction hit position valid",
                    octaryn_server_client_block_command_hit_position(
                        &place, &hit_position) != 0u);
  ok &= expect_equal("interaction hit x", hit_position.x, 0);
  ok &= expect_equal("interaction hit y", hit_position.y, 0);
  ok &= expect_equal("interaction hit z", hit_position.z, 0);
  ok &= expect_true("adjacent place interaction accepted",
                    octaryn_server_client_block_command_is_valid_interaction(
                        &place, uint16_t{1}, AirBlock) != 0u);
  ok &= expect_true("occupied place interaction rejected",
                    octaryn_server_client_block_command_is_valid_interaction(
                        &place, uint16_t{1}, uint16_t{2}) == 0u);

  octaryn_host_command far_place = place;
  far_place.x = 20.0f;
  ok &= expect_true("far interaction rejected",
                    octaryn_server_client_block_command_is_valid_interaction(
                        &far_place, uint16_t{1}, AirBlock) == 0u);

  octaryn_host_command break_hit =
      command(0, 0, 0, AirBlock, OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG);
  break_hit.x = 0.5f;
  break_hit.y = 0.5f;
  break_hit.z = 0.5f;
  break_hit.x2 = 0.0f;
  break_hit.y2 = 0.0f;
  break_hit.z2 = 0.0f;
  ok &= expect_true("break hit interaction accepted",
                    octaryn_server_client_block_command_is_valid_interaction(
                        &break_hit, uint16_t{1}, uint16_t{1}) != 0u);

  octaryn_host_command non_finite = place;
  non_finite.x = std::numeric_limits<float>::infinity();
  ok &= expect_true("non-finite hit position rejected",
                    octaryn_server_client_block_command_hit_position(
                        &non_finite, &hit_position) == 0u);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_positions();
  ok &= validate_edits_and_air_overrides();
  ok &= validate_snapshot_and_load();
  ok &= validate_clear_generated_matches();
  ok &= validate_block_edit_service();
  ok &= validate_block_command_validation();
  ok &= validate_change_queue();
  ok &= validate_command_queue();
  ok &= validate_client_interaction_policy();
  ok &= validate_chunk_stream();
  ok &= validate_chunk_stream_process_tick();

  if (!ok) {
    return 1;
  }

  std::puts("server block store native probe passed");
  return 0;
}
