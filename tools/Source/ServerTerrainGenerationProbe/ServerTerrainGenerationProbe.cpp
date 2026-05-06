#include "TerrainGeneration.h"

#include <cmath>
#include <cstdio>
#include <string_view>

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
  for (int x = -128; x <= 128 && !found_water_column; ++x) {
    for (int z = -128; z <= 128 && !found_water_column; ++z) {
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
  ok &= expect_equal("empty world air",
                     octaryn_server_empty_world_generated_block(0, 0, 0), Air);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_generated_blocks();
  ok &= validate_empty_world();

  if (!ok) {
    return 1;
  }

  std::puts("server terrain generation native probe passed");
  return 0;
}
