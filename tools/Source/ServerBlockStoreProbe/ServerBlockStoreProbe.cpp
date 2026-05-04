#include "BlockStore.h"

#include <cstdio>
#include <string_view>
#include <vector>

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

} // namespace

int main() {
  bool ok = true;
  ok &= validate_positions();
  ok &= validate_edits_and_air_overrides();
  ok &= validate_snapshot_and_load();
  ok &= validate_clear_generated_matches();

  if (!ok) {
    return 1;
  }

  std::puts("server block store native probe passed");
  return 0;
}
