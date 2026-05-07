#include "TerrainGeneration.h"

#include "BlockStore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

using octaryn::server::world::blocks::AirBlock;
using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;
using octaryn::server::world::blocks::WorldMaxYExclusive;
using octaryn::server::world::blocks::WorldMinY;

constexpr int32_t Seed = 1337;
constexpr uint16_t EmptyWorldWhiteBlock = 1u;

struct terrain_materials {
  uint16_t surface_block = AirBlock;
  uint16_t fill_block = AirBlock;
  bool has_grass_surface = false;
};

bool is_valid_position(int32_t y) {
  return y >= WorldMinY && y < WorldMaxYExclusive;
}

int32_t floor_mod(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}

uint32_t rotate_left(uint32_t value, int amount) {
  return (value << amount) | (value >> (32 - amount));
}

float hash_noise(int32_t x, int32_t z, int32_t seed_offset) {
  uint32_t value = static_cast<uint32_t>(Seed + seed_offset);
  value ^= static_cast<uint32_t>(x) * 0x9E3779B9u;
  value = rotate_left(value, 13);
  value ^= static_cast<uint32_t>(z) * 0x85EBCA6Bu;
  value ^= value >> 16u;
  value *= 0x7FEB352Du;
  value ^= value >> 15u;
  value *= 0x846CA68Bu;
  value ^= value >> 16u;
  return static_cast<float>(value) / static_cast<float>(UINT32_MAX) * 2.0f -
         1.0f;
}

float smooth_step(float value) { return value * value * (3.0f - 2.0f * value); }

float lerp(float start, float end, float amount) {
  return start + (end - start) * amount;
}

float smooth_value_noise(float x, float z, int32_t seed_offset) {
  const auto x0 = static_cast<int32_t>(std::floor(x));
  const auto z0 = static_cast<int32_t>(std::floor(z));
  const float tx = smooth_step(x - static_cast<float>(x0));
  const float tz = smooth_step(z - static_cast<float>(z0));
  const float a = lerp(hash_noise(x0, z0, seed_offset),
                       hash_noise(x0 + 1, z0, seed_offset), tx);
  const float b = lerp(hash_noise(x0, z0 + 1, seed_offset),
                       hash_noise(x0 + 1, z0 + 1, seed_offset), tx);
  return lerp(a, b, tz);
}

float sample_fbm(int32_t world_x, int32_t world_z, float frequency,
                 int32_t octaves, int32_t seed_offset) {
  float amplitude = 1.0f;
  float amplitude_total = 0.0f;
  float value = 0.0f;
  float x = static_cast<float>(world_x) * frequency;
  float z = static_cast<float>(world_z) * frequency;

  for (int32_t octave = 0; octave < octaves; ++octave) {
    value += smooth_value_noise(x, z, seed_offset + octave * 9973) * amplitude;
    amplitude_total += amplitude;
    amplitude *= 0.5f;
    x *= 2.0f;
    z *= 2.0f;
  }

  return amplitude_total > 0.0f ? value / amplitude_total : 0.0f;
}

OctarynServerTerrainColumnSample sample_column(int32_t world_x,
                                               int32_t world_z) {
  return OctarynServerTerrainColumnSample{
      .world_x = world_x,
      .world_z = world_z,
      .local_x = floor_mod(world_x, ChunkWidth),
      .local_z = floor_mod(world_z, ChunkDepth),
      .local_width = ChunkWidth,
      .local_depth = ChunkDepth,
      .max_terrain_y = WorldMaxYExclusive - 1,
      .height_noise = sample_fbm(world_x, world_z, 0.005f, 6, 0),
      .lowland_noise = sample_fbm(-world_x, world_z, 0.01f, 6, 101),
      .biome_noise = sample_fbm(world_x, world_z, 0.2f, 6, 211),
  };
}

float compute_height(const OctarynServerTerrainColumnSample &sample,
                     bool &is_lowland) {
  float height =
      std::pow(std::max(sample.height_noise * 50.0f, 0.0f), 1.3f) + 30.0f;
  height = std::clamp(height, 0.0f, static_cast<float>(sample.max_terrain_y));

  is_lowland = false;
  if (height < 40.0f) {
    height += sample.lowland_noise * 12.0f;
    is_lowland = true;
  }

  return height;
}

terrain_materials
classify_materials(float height, float biome,
                   const OctarynServerTerrainMaterialRules &rules) {
  if (height + biome < 31.0f) {
    return terrain_materials{
        .surface_block = rules.sand_block,
        .fill_block = rules.sand_block,
        .has_grass_surface = false,
    };
  }

  biome = std::clamp(biome * 8.0f, -5.0f, 5.0f);
  if (height + biome < 61.0f) {
    return terrain_materials{
        .surface_block = rules.grass_block,
        .fill_block = rules.dirt_block,
        .has_grass_surface = true,
    };
  }

  if (height + biome < 132.0f) {
    return terrain_materials{
        .surface_block = rules.stone_block,
        .fill_block = rules.stone_block,
        .has_grass_surface = false,
    };
  }

  return terrain_materials{
      .surface_block = rules.snow_block,
      .fill_block = rules.stone_block,
      .has_grass_surface = false,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_terrain_plan_column(
    int32_t x, int32_t z, const OctarynServerTerrainMaterialRules *rules,
    OctarynServerTerrainColumnPlan *plan) {
  if (rules == nullptr || plan == nullptr) {
    return -1;
  }

  const OctarynServerTerrainColumnSample sample = sample_column(x, z);
  bool is_lowland = false;
  const float height = compute_height(sample, is_lowland);
  const terrain_materials materials =
      classify_materials(height, sample.biome_noise, *rules);

  const auto terrain_height = static_cast<int32_t>(std::ceil(height));
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
      .is_lowland = is_lowland ? 1u : 0u,
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

  *block = AirBlock;
  if (!is_valid_position(y)) {
    return 0;
  }

  OctarynServerTerrainColumnPlan column{};
  if (octaryn_server_terrain_plan_column(x, z, rules, &column) != 0) {
    return -1;
  }

  if (y < column.terrain_height) {
    *block = column.fill_block;
  } else if (y == column.terrain_height) {
    *block = column.surface_block;
  } else if (y < rules->water_height) {
    *block = rules->water_block;
  }

  return 0;
}

uint16_t octaryn_server_empty_world_generated_block(int32_t x, int32_t y,
                                                    int32_t z) {
  (void)x;
  (void)z;
  return y >= WorldMinY && y < 0 ? EmptyWorldWhiteBlock : AirBlock;
}

uint16_t octaryn_server_empty_world_white_block() {
  return EmptyWorldWhiteBlock;
}

int32_t octaryn_server_terrain_clear_matching_overrides(
    void *block_store, const OctarynServerTerrainMaterialRules *rules) {
  auto *store = static_cast<octaryn::server::world::blocks::BlockStore *>(
      block_store);
  if (store == nullptr || rules == nullptr) {
    return 0;
  }

  return store->clear_overrides_matching(
      [rules](const octaryn::server::world::blocks::BlockPosition &position) {
        uint16_t block = AirBlock;
        (void)octaryn_server_terrain_generated_block(
            position.x, position.y, position.z, rules, &block);
        return block;
      });
}

int32_t octaryn_server_empty_world_clear_matching_overrides(void *block_store) {
  auto *store = static_cast<octaryn::server::world::blocks::BlockStore *>(
      block_store);
  if (store == nullptr) {
    return 0;
  }

  return store->clear_overrides_matching(
      [](const octaryn::server::world::blocks::BlockPosition &position) {
        return octaryn_server_empty_world_generated_block(position.x,
                                                         position.y,
                                                         position.z);
      });
}
}
