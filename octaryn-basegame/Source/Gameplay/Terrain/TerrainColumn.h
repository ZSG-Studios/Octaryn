#pragma once

#include "TerrainNoise.h"

#include <algorithm>

namespace octaryn::basegame::terrain {

inline constexpr int32_t ChunkWidth = 32, ChunkDepth = 32;
inline constexpr int32_t WorldMinY = -256, WorldMaxYExclusive = 256;
inline constexpr uint16_t AirBlock = 0;

enum class Biome { Ocean, Beach, Plains, Forest, Desert, Alpine };

struct TerrainColumnSample {
  int32_t world_x{}, world_z{}, local_x{}, local_z{};
  int32_t local_width{ChunkWidth}, local_depth{ChunkDepth};
  int32_t terrain_height{};
  double continentalness{}, erosion{}, ridge{}, temperature{}, humidity{}, river{};
  bool is_lowland{};
};

struct terrain_materials {
  uint16_t surface_block{}, fill_block{};
  bool has_grass_surface{};
};

inline int32_t floor_mod(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}

inline double ramp(double start, double end, double value) {
  const double t = std::clamp((value - start) / (end - start), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

inline TerrainColumnSample sample_column(int32_t world_x, int32_t world_z) {
  // Double coordinates preserve cell interpolation far beyond the float precision limit.
  const double x = world_x, z = world_z;
  const double wx = x + 68 * fbm(x, z, 0.0017, 11, 3);
  const double wz = z + 68 * fbm(x, z, 0.0017, 29, 3);
  TerrainColumnSample sample;
  sample.world_x = world_x;
  sample.world_z = world_z;
  sample.local_x = floor_mod(world_x, ChunkWidth);
  sample.local_z = floor_mod(world_z, ChunkDepth);
  sample.continentalness = fbm(wx, wz, 0.0008, 43);
  sample.erosion = fbm(wx, wz, 0.0021, 71);
  sample.ridge = 1 - std::abs(fbm(wx, wz, 0.0036, 97));
  sample.temperature = fbm(x, z, 0.0006, 131, 3);
  sample.humidity = fbm(x, z, 0.0008, 173, 3);
  sample.river = 1 - ramp(0.025, 0.095, std::abs(fbm(wx, wz, 0.0015, 223, 3)));

  const double land = ramp(-0.32, 0.22, sample.continentalness);
  const double mountains = ramp(-0.05, 0.45, sample.continentalness) *
                           (1 - ramp(-0.35, 0.35, sample.erosion));
  const double peaks = std::pow(ramp(0.35, 0.98, sample.ridge), 3);
  double height = lerp(5, 52, land) + 135 * mountains * peaks;
  height += fbm(wx, wz, 0.012, 269) * lerp(3, 12, land);
  // Only cut existing land: a river must never raise the ocean floor.
  height = lerp(height, std::min(height, 25.0), sample.river * land);
  sample.terrain_height = static_cast<int32_t>(std::ceil(std::clamp(height, -220.0, 240.0)));
  sample.is_lowland = sample.terrain_height < 65 && mountains < 0.3;
  return sample;
}

template <typename Rules>
inline Biome classify_biome(const TerrainColumnSample& sample, const Rules& rules) {
  if (sample.terrain_height < rules.water_height - 2) return Biome::Ocean;
  if (sample.terrain_height <= rules.water_height + 2) return Biome::Beach;
  const double temperature = sample.temperature - std::max(0, sample.terrain_height - 60) * 0.007;
  if (temperature < -0.38 || sample.terrain_height > 150) return Biome::Alpine;
  if (sample.temperature > 0.18 && sample.humidity < -0.1) return Biome::Desert;
  return sample.humidity > 0.08 ? Biome::Forest : Biome::Plains;
}

template <typename Rules>
inline terrain_materials classify_materials(const TerrainColumnSample& sample, const Rules& rules) {
  switch (classify_biome(sample, rules)) {
  case Biome::Ocean: case Biome::Beach: case Biome::Desert:
    return {rules.sand_block, rules.sand_block, false};
  case Biome::Alpine:
    return {rules.snow_block, rules.stone_block, false};
  default:
    if (sample.terrain_height > 105) return {rules.stone_block, rules.stone_block, false};
    return {rules.grass_block, rules.dirt_block, true};
  }
}

} // namespace octaryn::basegame::terrain
