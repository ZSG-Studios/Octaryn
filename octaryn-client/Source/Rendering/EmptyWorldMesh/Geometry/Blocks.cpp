#include "EmptyWorldMesh.h"

#include "Packing.h"

#include <algorithm>
#include <cmath>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::has_block_override;
using octaryn_client_app::world_block_record;

namespace {

constexpr int32_t kTerrainWaterHeight = 30;
constexpr int32_t kTerrainMaxY = 255;
constexpr uint16_t kBlockAir = 0u;
constexpr uint16_t kBlockGrass = 1u;
constexpr uint16_t kBlockDirt = 2u;
constexpr uint16_t kBlockSand = 3u;
constexpr uint16_t kBlockSnow = 4u;
constexpr uint16_t kBlockStone = 5u;
constexpr uint16_t kBlockWater = 14u;

float lerp(float start, float end, float amount) {
  return start + (end - start) * amount;
}

float smooth_step(float value) {
  return value * value * (3.0f - 2.0f * value);
}

float hash_noise(int32_t x, int32_t z, int32_t seed_offset) {
  uint32_t value = static_cast<uint32_t>(1337 + seed_offset);
  value ^= static_cast<uint32_t>(x) * 0x9E3779B9u;
  value = (value << 13u) | (value >> 19u);
  value ^= static_cast<uint32_t>(z) * 0x85EBCA6Bu;
  value ^= value >> 16u;
  value *= 0x7FEB352Du;
  value ^= value >> 15u;
  value *= 0x846CA68Bu;
  value ^= value >> 16u;
  return static_cast<float>(value) / 4294967295.0f * 2.0f - 1.0f;
}

float smooth_value_noise(float x, float z, int32_t seed_offset) {
  const int32_t x0 = static_cast<int32_t>(std::floor(x));
  const int32_t z0 = static_cast<int32_t>(std::floor(z));
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

} // namespace

empty_world_terrain_column empty_world_seed_column(int32_t world_x,
                                                   int32_t world_z) {
  float height = std::pow(std::max(sample_fbm(world_x, world_z, 0.005f, 6, 0) *
                                       50.0f,
                                   0.0f),
                          1.3f) +
                 30.0f;
  height = std::clamp(height, 0.0f, static_cast<float>(kTerrainMaxY));
  if (height < 40.0f) {
    height += sample_fbm(-world_x, world_z, 0.01f, 6, 101) * 12.0f;
  }

  float biome = sample_fbm(world_x, world_z, 0.2f, 6, 211);
  if (height + biome < 31.0f) {
    return empty_world_terrain_column{static_cast<int32_t>(std::ceil(height)),
                                      kBlockSand, kBlockSand};
  }

  biome = std::clamp(biome * 8.0f, -5.0f, 5.0f);
  if (height + biome < 61.0f) {
    return empty_world_terrain_column{static_cast<int32_t>(std::ceil(height)),
                                      kBlockGrass, kBlockDirt};
  }
  if (height + biome < 132.0f) {
    return empty_world_terrain_column{static_cast<int32_t>(std::ceil(height)),
                                      kBlockStone, kBlockStone};
  }
  return empty_world_terrain_column{static_cast<int32_t>(std::ceil(height)),
                                    kBlockSnow, kBlockStone};
}

uint32_t empty_world_block_atlas_layer(uint16_t block, uint32_t direction) {
  switch (block) {
  case kBlockGrass:
    return direction == 4u ? 1u : direction == 5u ? 3u : 2u;
  case kBlockDirt:
    return 3u;
  case kBlockSand:
    return 5u;
  case kBlockSnow:
    return 6u;
  case kBlockStone:
    return 4u;
  case kBlockWater:
    return 16u;
  default:
    return 0u;
  }
}

uint16_t empty_world_generated_block(const block_position_key &key) {
  if (key.y < kEmptyWorldMinY || key.y >= kEmptyWorldMaxYExclusive) {
    return kBlockAir;
  }

  const empty_world_terrain_column column = empty_world_seed_column(key.x, key.z);
  if (key.y < column.height) {
    return column.fill;
  }
  if (key.y == column.height) {
    return column.surface;
  }
  return key.y < kTerrainWaterHeight ? kBlockWater : kBlockAir;
}

uint16_t empty_world_effective_block(const block_lookup &overrides,
                                     const block_position_key &key) {
  uint16_t block = 0u;
  return has_block_override(overrides, key, block)
             ? block
             : empty_world_generated_block(key);
}

void apply_empty_world_overrides_from_records(
    const std::vector<world_block_record> &records, block_lookup &overrides) {
  overrides.clear();
  for (const world_block_record &record : records) {
    if (record.y < kEmptyWorldMinY || record.y >= kEmptyWorldMaxYExclusive) {
      continue;
    }

    overrides[block_position_key{record.x, record.y, record.z}] = record.block;
  }
}
