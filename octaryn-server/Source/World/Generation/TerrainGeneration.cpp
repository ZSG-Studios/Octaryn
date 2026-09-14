#include "TerrainGeneration.h"

#include "BlockStore.h"
#include "TerrainDensity.h"
#include "TerrainColumnCache.h"
#include "TerrainVegetation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

using octaryn::server::world::blocks::AirBlock;
using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;
using octaryn::server::world::blocks::WorldMaxYExclusive;
using octaryn::server::world::blocks::WorldMinY;

constexpr uint16_t EmptyWorldWhiteBlock = 1u;
constexpr int32_t FlatTestSurfaceY = 38;
using octaryn::server::world::generation::cached_column;
using octaryn::basegame::terrain::classify_materials;
using octaryn::basegame::terrain::terrain_materials;

bool is_valid_position(int32_t y) {
  return y >= WorldMinY && y < WorldMaxYExclusive;
}

} // namespace

extern "C" {

int32_t octaryn_server_terrain_plan_column(
    int32_t x, int32_t z, const OctarynServerTerrainMaterialRules *rules,
    OctarynServerTerrainColumnPlan *plan) {
  if (rules == nullptr || plan == nullptr) {
    return -1;
  }

  const auto sample = cached_column(x, z);
  const terrain_materials materials =
      classify_materials(sample, *rules);

  const auto terrain_height = sample.terrain_height;
  *plan = OctarynServerTerrainColumnPlan{
      .world_x = sample.world_x,
      .world_z = sample.world_z,
      .local_x = sample.local_x,
      .local_z = sample.local_z,
      .local_width = sample.local_width,
      .local_depth = sample.local_depth,
      .terrain_height = terrain_height,
      .decoration_y = std::max(terrain_height, rules->water_height),
      .surface_block = materials.surface_block,
      .fill_block = materials.fill_block,
      .is_lowland = sample.is_lowland ? 1u : 0u,
      .has_grass_surface = materials.has_grass_surface ? 1u : 0u,
  };
  return 0;
}

int32_t octaryn_server_terrain_generated_block(
    int32_t x, int32_t y, int32_t z,
    const OctarynServerTerrainMaterialRules *rules, uint16_t *block) {
  if (block == nullptr || rules == nullptr) {
    return -1;
  }
  if (rules->generator_revision != 2 && rules->generator_revision != 3) return -1;

  *block = AirBlock;
  if (!is_valid_position(y)) {
    return 0;
  }

  *block = octaryn::basegame::terrain::sample_block(cached_column(x, z), y, *rules);
  if (rules->generator_revision == 3)
    *block = octaryn::basegame::terrain::sample_vegetation(x, y, z, *block, *rules, cached_column);

  return 0;
}

uint16_t octaryn_server_empty_world_generated_block(int32_t x, int32_t y,
                                                    int32_t z) {
  (void)x;
  (void)z;
  return y >= WorldMinY && y < 0 ? EmptyWorldWhiteBlock : AirBlock;
}

uint16_t octaryn_server_flat_test_generated_block(
    int32_t x, int32_t y, int32_t z,
    const OctarynServerTerrainMaterialRules *rules) {
  (void)x;
  (void)z;
  if (rules == nullptr || !is_valid_position(y)) {
    return AirBlock;
  }

  if (y < FlatTestSurfaceY) {
    return rules->dirt_block;
  }
  if (y == FlatTestSurfaceY) {
    return rules->grass_block;
  }
  return AirBlock;
}

uint16_t octaryn_server_empty_world_white_block() {
  return EmptyWorldWhiteBlock;
}

int32_t octaryn_server_terrain_clear_matching_overrides(
    void *block_store, const OctarynServerTerrainMaterialRules *rules) {
  if (block_store == nullptr || rules == nullptr) {
    return 0;
  }

  auto context = *rules;
  return octaryn_server_block_store_clear_matching_overrides(
      block_store,
      [](void *context, const octaryn_server_block_position *position) {
        const auto *rules =
            static_cast<const OctarynServerTerrainMaterialRules *>(context);
        uint16_t block = AirBlock;
        (void)octaryn_server_terrain_generated_block(
            position->x, position->y, position->z, rules, &block);
        return block;
      },
      &context);
}

int32_t octaryn_server_empty_world_clear_matching_overrides(void *block_store) {
  return octaryn_server_block_store_clear_matching_overrides(
      block_store,
      [](void *, const octaryn_server_block_position *position) {
        return octaryn_server_empty_world_generated_block(position->x,
                                                         position->y,
                                                         position->z);
      },
      nullptr);
}

int32_t octaryn_server_flat_test_clear_matching_overrides(
    void *block_store, const OctarynServerTerrainMaterialRules *rules) {
  if (block_store == nullptr || rules == nullptr) {
    return 0;
  }

  auto context = *rules;
  return octaryn_server_block_store_clear_matching_overrides(
      block_store,
      [](void *context, const octaryn_server_block_position *position) {
        const auto *rules =
            static_cast<const OctarynServerTerrainMaterialRules *>(context);
        return octaryn_server_flat_test_generated_block(
            position->x, position->y, position->z, rules);
      },
      &context);
}
}
