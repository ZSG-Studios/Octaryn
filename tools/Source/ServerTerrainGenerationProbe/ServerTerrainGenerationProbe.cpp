#include "TerrainGeneration.h"
#include "BlockStore.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

bool validate_column_cache();
bool validate_terrain_features();

namespace {

constexpr uint16_t Air = 0u;
constexpr uint16_t White = 1u;
constexpr uint16_t Sand = 3u;
constexpr uint16_t Grass = 1u;
constexpr uint16_t Dirt = 2u;
constexpr uint16_t Stone = 5u;
constexpr uint16_t Snow = 4u;
constexpr uint16_t Water = 14u;

constexpr OctarynServerTerrainMaterialRules BasegameRules{
    .water_height = 30,
    .water_block = Water,
    .sand_block = Sand,
    .grass_block = Grass,
    .dirt_block = Dirt,
    .stone_block = Stone,
    .snow_block = Snow,
};

using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockPosition;
using octaryn::server::world::blocks::BlockStore;

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

bool validate_generated_blocks() {
  uint16_t block = 99u;
  OctarynServerTerrainColumnPlan column{};
  OctarynServerTerrainColumnPlan water_column{};
  bool ok = true;
  ok &= expect_equal(
      "column plan result",
      octaryn_server_terrain_plan_column(-1, -33, &BasegameRules, &column), 0);
  ok &= expect_equal("floor local x", column.local_x, 31);
  ok &= expect_equal("floor local z", column.local_z, 31);
  ok &= expect_true("terrain height valid",
                    column.terrain_height >= 0 && column.terrain_height < 512);

  ok &= expect_equal(
      "fill block result",
      octaryn_server_terrain_generated_block(-1, column.terrain_height - 1, -33,
                                             &BasegameRules, &block),
      0);
  ok &= expect_equal("fill block", block, column.fill_block);

  ok &=
      expect_equal("surface block result",
                   octaryn_server_terrain_generated_block(
                       -1, column.terrain_height, -33, &BasegameRules, &block),
                   0);
  ok &= expect_equal("surface block", block, column.surface_block);

  bool found_water_column = false;
  for (int x = -2048; x <= 2048 && !found_water_column; x += 16) {
    for (int z = -2048; z <= 2048 && !found_water_column; z += 16) {
      if (octaryn_server_terrain_plan_column(x, z, &BasegameRules,
                                             &water_column) == 0 &&
          water_column.terrain_height + 1 < BasegameRules.water_height) {
        found_water_column = true;
      }
    }
  }
  ok &= expect_true("water column found", found_water_column);
  if (found_water_column) {
    ok &=
        expect_equal("water block result",
                     octaryn_server_terrain_generated_block(
                         water_column.world_x, water_column.terrain_height + 1,
                         water_column.world_z, &BasegameRules, &block),
                     0);
    ok &= expect_equal("water block", block, Water);
  }

  ok &= expect_equal(
      "air block result",
      octaryn_server_terrain_generated_block(0, 512, 0, &BasegameRules, &block),
      0);
  ok &= expect_equal("air block", block, Air);
  return ok;
}

bool validate_empty_world() {
  bool ok = true;
  ok &=
      expect_equal("empty world solid",
                   octaryn_server_empty_world_generated_block(0, -1, 0), White);
  ok &= expect_equal("empty world white block",
                     octaryn_server_empty_world_white_block(), White);
  ok &= expect_equal("empty world air",
                     octaryn_server_empty_world_generated_block(0, 0, 0), Air);
  return ok;
}

bool validate_terrain_volume() {
  bool ok = true;
  std::size_t caves = 0, deep_stone = 0, water = 0, samples = 0;
  int minimum_height = 255, maximum_height = -256;
  for (int z = -512; z <= 512; z += 32) {
    for (int x = -512; x <= 512; x += 32) {
      OctarynServerTerrainColumnPlan column{};
      ok &= expect_equal("volume column plan",
          octaryn_server_terrain_plan_column(x, z, &BasegameRules, &column), 0);
      minimum_height = std::min(minimum_height, column.terrain_height);
      maximum_height = std::max(maximum_height, column.terrain_height);
      for (int y = -256; y < 256; y += 4) {
        uint16_t block{};
        ok &= expect_equal("volume generated block",
            octaryn_server_terrain_generated_block(x, y, z, &BasegameRules, &block), 0);
        ++samples;
        if (y < -252) ok &= expect_equal("solid bottom stone", block, Stone);
        else if (y < column.terrain_height - 8) {
          ok &= expect_true("deep terrain is stone or cave", block == Stone || block == Air);
          caves += block == Air ? 1u : 0u;
          deep_stone += block == Stone ? 1u : 0u;
        }
        if (y > column.terrain_height) {
          const auto expected = y < BasegameRules.water_height ? Water : Air;
          ok &= expect_equal("above surface sea level", block, expected);
          water += block == Water ? 1u : 0u;
        }
      }
      for (int depth = 0; depth <= 8; ++depth) {
        uint16_t block{};
        const auto y = column.terrain_height - depth;
        ok &= expect_equal("protected roof sample",
            octaryn_server_terrain_generated_block(x, y, z, &BasegameRules, &block), 0);
        ok &= expect_true("protected surface roof", block != Air && block != Water);
        if (depth == 0) ok &= expect_equal("surface plan matches voxel", block, column.surface_block);
        if (depth == 1) ok &= expect_equal("surface fill matches voxel", block, column.fill_block);
      }
    }
  }
  ok &= expect_true("underground caves occur", caves > 0);
  ok &= expect_true("deep stone remains", deep_stone > caves);
  ok &= expect_true("ocean water occurs", water > 0);
  ok &= expect_true("terrain relief occurs", maximum_height - minimum_height > 32);
  std::printf("terrain_volume samples=%zu caves=%zu deep_stone=%zu water=%zu height_min=%d height_max=%d\n",
      samples, caves, deep_stone, water, minimum_height, maximum_height);
  return ok;
}

bool validate_bounds_and_order() {
  bool ok = true;
  constexpr std::array coordinates{-2147483647 - 1, -32000000, -33, -32, -1,
      0, 31, 32, 33, 32000000, 2147483647};
  std::vector<BlockPosition> positions;
  std::vector<uint16_t> expected;
  for (const auto x : coordinates) for (const auto z : coordinates) {
    OctarynServerTerrainColumnPlan column{};
    ok &= expect_equal("signed column plan",
        octaryn_server_terrain_plan_column(x, z, &BasegameRules, &column), 0);
    ok &= expect_true("signed local coordinates",
        column.local_x >= 0 && column.local_x < 32 && column.local_z >= 0 && column.local_z < 32);
    ok &= expect_true("terrain within vertical bounds", column.terrain_height >= -252 && column.terrain_height < 256);
    for (const auto y : std::array{std::numeric_limits<int>::min(), -257, -256,
             -253, -252, -192, -64, -1, 0, column.terrain_height - 9,
             column.terrain_height, 255, 256, std::numeric_limits<int>::max()}) {
      uint16_t block{99};
      ok &= expect_equal("bounded block sample",
          octaryn_server_terrain_generated_block(x, y, z, &BasegameRules, &block), 0);
      if (y < -256 || y >= 256) ok &= expect_equal("outside world is air", block, Air);
      if (y >= -256 && y < -252) ok &= expect_equal("bottom four layers stone", block, Stone);
      positions.push_back({x, y, z});
      expected.push_back(block);
    }
  }
  std::vector<std::size_t> order(positions.size());
  std::iota(order.begin(), order.end(), std::size_t{});
  std::mt19937 random(5719);
  std::shuffle(order.begin(), order.end(), random);
  for (const auto index : order) {
    const auto position = positions[index];
    uint16_t block{};
    ok &= expect_equal("shuffled sample result",
        octaryn_server_terrain_generated_block(position.x, position.y, position.z, &BasegameRules, &block), 0);
    ok &= expect_equal("order independent terrain", block, expected[index]);
  }
  uint16_t block{};
  OctarynServerTerrainColumnPlan column{};
  ok &= expect_equal("null block output", octaryn_server_terrain_generated_block(0, 0, 0, &BasegameRules, nullptr), -1);
  ok &= expect_equal("null sample rules", octaryn_server_terrain_generated_block(0, 0, 0, nullptr, &block), -1);
  ok &= expect_equal("null plan output", octaryn_server_terrain_plan_column(0, 0, &BasegameRules, nullptr), -1);
  ok &= expect_equal("null plan rules", octaryn_server_terrain_plan_column(0, 0, nullptr, &column), -1);
  std::printf("terrain_bounds_order samples=%zu\n", positions.size());
  return ok;
}

bool validate_native_override_cleanup() {
  bool ok = true;
  BlockStore terrain_store;
  OctarynServerTerrainColumnPlan column{};
  uint16_t generated = Air;
  ok &= expect_equal("cleanup column plan",
                     octaryn_server_terrain_plan_column(0, 0, &BasegameRules,
                                                        &column),
                     0);
  ok &= expect_equal(
      "cleanup generated block",
      octaryn_server_terrain_generated_block(0, column.terrain_height, 0,
                                             &BasegameRules, &generated),
      0);
  terrain_store.set_block(BlockEdit{
      .position = BlockPosition{.x = 0, .y = column.terrain_height, .z = 0},
      .block = generated,
  });
  const BlockPosition removed{.x = 0, .y = -256, .z = 0};
  terrain_store.set_block(BlockEdit{.position = removed, .block = Air}, true);
  const BlockPosition placed{.x = 0, .y = 255, .z = 0};
  terrain_store.set_block(BlockEdit{.position = placed, .block = Stone});

  ok &= expect_equal(
      "terrain cleanup count",
      octaryn_server_terrain_clear_matching_overrides(&terrain_store,
                                                      &BasegameRules),
      1);
  ok &= expect_equal("terrain cleanup remaining",
                     static_cast<int>(terrain_store.block_count()), 2);
  uint16_t preserved{99};
  ok &= expect_true("air removal override retained", terrain_store.try_get_block(removed, preserved));
  ok &= expect_equal("air removal value retained", preserved, Air);
  ok &= expect_true("placement override retained", terrain_store.try_get_block(placed, preserved));
  ok &= expect_equal("placement value retained", preserved, Stone);
  ok &= expect_equal("cleanup is idempotent",
      octaryn_server_terrain_clear_matching_overrides(&terrain_store, &BasegameRules), 0);

  BlockStore empty_store;
  empty_store.set_block(BlockEdit{
      .position = BlockPosition{.x = 0, .y = -1, .z = 0},
      .block = White,
  });
  empty_store.set_block(BlockEdit{
      .position = BlockPosition{.x = 0, .y = 1, .z = 0},
      .block = Stone,
  });

  ok &= expect_equal(
      "empty cleanup count",
      octaryn_server_empty_world_clear_matching_overrides(&empty_store), 1);
  ok &= expect_equal("empty cleanup remaining",
                     static_cast<int>(empty_store.block_count()), 1);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_generated_blocks();
  ok &= validate_empty_world();
  ok &= validate_terrain_volume();
  ok &= validate_bounds_and_order();
  ok &= validate_native_override_cleanup();
  ok &= validate_column_cache();
  ok &= validate_terrain_features();

  if (!ok) {
    return 1;
  }

  std::puts("server terrain generation native probe passed");
  return 0;
}
