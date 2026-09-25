#pragma once
#include "TerrainColumn.h"
#include <limits>

namespace octaryn::basegame::terrain {

// Basegame catalog IDs, shared by authoritative queries and client reconstruction.
inline constexpr uint16_t LogBlock = 6, LeavesBlock = 7, BushBlock = 9;
inline constexpr uint16_t FlowerBlocks[] = {10, 11, 13, 12};
inline constexpr int VegetationRadius = 1;

inline bool tree_anchor(int32_t x, int32_t z) {
  const int64_t cell_x = (int64_t(x) - floor_mod(x, 7)) / 7;
  const int64_t cell_z = (int64_t(z) - floor_mod(z, 7)) / 7;
  const int offset_x = 1 + int((lattice(cell_x, 0, cell_z, 701) + 1) * 2.5);
  const int offset_z = 1 + int((lattice(cell_x, 0, cell_z, 709) + 1) * 2.5);
  return floor_mod(x, 7) == offset_x && floor_mod(z, 7) == offset_z;
}

template<class Rules, class GetColumn, class Emit>
void emit_vegetation(int32_t x, int32_t z, const Rules& rules,
                     GetColumn&& get_column, Emit&& emit) {
  const bool tree = tree_anchor(x, z);
  const double chance = (lattice(x, 0, z, 727) + 1) * 0.5;
  if (!tree && chance >= 0.14) return;
  const auto column = get_column(x, z);
  if (!classify_materials(column, rules).has_grass_surface ||
      column.terrain_height <= rules.water_height || column.terrain_height > 240) return;
  const bool forest = classify_biome(column, rules) == Biome::Forest;
  const auto put = [&](int dx, int dy, int dz, uint16_t block) {
    const int64_t px = int64_t(x) + dx, pz = int64_t(z) + dz;
    const int y = column.terrain_height + dy;
    if (px < INT32_MIN || px > INT32_MAX || pz < INT32_MIN || pz > INT32_MAX ||
        y < WorldMinY || y >= WorldMaxYExclusive) return;
    emit(int32_t(px), y, int32_t(pz), block);
  };
  if (tree && chance < (forest ? 0.8 : 0.22)) {
    const int height = 4 + (lattice(x, 0, z, 733) > 0 ? 1 : 0);
    for (int dy = 1; dy <= height; ++dy) put(0, dy, 0, LogBlock);
    // Recovered original canopy silhouette; anchors are world-aligned, including seams.
    for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz)
      for (int dy = 0; dy < 2; ++dy)
        if (dx || dz || dy) put(dx, height + dy, dz, LeavesBlock);
  } else if (chance < 0.11) {
    put(0, 1, 0, BushBlock);
  } else if (chance < 0.14) {
    const unsigned flower = unsigned((lattice(x, 0, z, 739) + 1) * 2);
    put(0, 1, 0, FlowerBlocks[std::min(flower, 3u)]);
  }
}

inline uint16_t merge_vegetation(uint16_t current, uint16_t feature) {
  if (feature == LogBlock || current == AirBlock) return feature;
  if (feature == LeavesBlock && current != LogBlock) return feature;
  return current;
}

template<class Rules, class GetColumn>
uint16_t sample_vegetation(int32_t x, int32_t y, int32_t z,
                           uint16_t terrain, const Rules& rules, GetColumn&& get_column) {
  if (terrain != AirBlock || y <= rules.water_height || y > 246) return terrain;
  uint16_t result = AirBlock;
  for (int64_t ax = std::max(int64_t(INT32_MIN), int64_t(x) - VegetationRadius);
       ax <= std::min(int64_t(INT32_MAX), int64_t(x) + VegetationRadius); ++ax)
    for (int64_t az = std::max(int64_t(INT32_MIN), int64_t(z) - VegetationRadius);
         az <= std::min(int64_t(INT32_MAX), int64_t(z) + VegetationRadius); ++az)
      emit_vegetation(int32_t(ax), int32_t(az), rules, get_column,
          [&](int32_t fx, int fy, int32_t fz, uint16_t block) {
            if (fx == x && fy == y && fz == z) result = merge_vegetation(result, block);
          });
  return result;
}
}
