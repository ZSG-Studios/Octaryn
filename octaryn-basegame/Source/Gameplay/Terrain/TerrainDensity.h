#pragma once

#include "TerrainColumn.h"

namespace octaryn::basegame::terrain {

inline constexpr std::array<double,3> CaveScaleXZ{0.022,0.036,0.031};
inline constexpr std::array<std::uint32_t,3> CaveChannel{307,353,401};

// Positive density is solid. A protected roof keeps column tops authoritative.
template <typename Noise>
inline double cave_density_with_noise(const TerrainColumnSample& column,int32_t y,Noise&& noise) {
  const int32_t depth = column.terrain_height - y;
  if (depth <= 8 || y < WorldMinY + 4) return 1;
  const double roof = ramp(8, 24, depth);
  const double floor = ramp(WorldMinY + 4, WorldMinY + 20, y);
  const double opening = roof * floor;
  const double chamber = noise(0,y * 0.031);
  const double tunnels_a = noise(1,y * 0.027);
  const double tunnels_b = noise(2,y * 0.033);
  const double tunnels = std::max(std::abs(tunnels_a), std::abs(tunnels_b));
  return std::min(0.64 - chamber * opening, tunnels - 0.085 * opening);
}

inline double cave_density(const TerrainColumnSample& column,int32_t y) {
  const double x=static_cast<double>(column.world_x),z=static_cast<double>(column.world_z);
  return cave_density_with_noise(column,y,[=](unsigned index,double py) {
    return noise3(x*CaveScaleXZ[index],py,z*CaveScaleXZ[index],CaveChannel[index]);
  });
}

// A sampler owns its immutable column geometry; mutable plane slots are local
// to this generation line and must not be shared concurrently between threads.
class CaveColumnSampler {
  TerrainColumnSample column_;
  std::array<Noise3Column,3> noise_;
public:
  explicit CaveColumnSampler(const TerrainColumnSample& column):column_(column),noise_{
      Noise3Column(double(column.world_x)*CaveScaleXZ[0],double(column.world_z)*CaveScaleXZ[0],CaveChannel[0]),
      Noise3Column(double(column.world_x)*CaveScaleXZ[1],double(column.world_z)*CaveScaleXZ[1],CaveChannel[1]),
      Noise3Column(double(column.world_x)*CaveScaleXZ[2],double(column.world_z)*CaveScaleXZ[2],CaveChannel[2])} {}
  const TerrainColumnSample& column() const {return column_;}
  double density(int32_t y) const {
    return cave_density_with_noise(column_,y,[this](unsigned index,double py) {return noise_[index].sample(py);});
  }
};

template <typename Rules,typename Density>
inline uint16_t sample_block_with_density(const TerrainColumnSample& column,int32_t y,
    const Rules& rules,const terrain_materials& materials,Density&& density) {
  if (y < WorldMinY || y >= WorldMaxYExclusive) return AirBlock;
  if (y > column.terrain_height) return y < rules.water_height ? rules.water_block : AirBlock;
  if (y == column.terrain_height) return materials.surface_block;
  if (y < WorldMinY + 4) return rules.stone_block;
  if (density() < 0) return AirBlock;
  return column.terrain_height - y <= 4 ? materials.fill_block : rules.stone_block;
}

template <typename Rules>
inline uint16_t sample_block(const TerrainColumnSample& column,int32_t y,
    const Rules& rules,const terrain_materials& materials) {
  return sample_block_with_density(column,y,rules,materials,[&] {return cave_density(column,y);});
}

template <typename Rules>
inline uint16_t sample_block_cached(const CaveColumnSampler& sampler,int32_t y,
    const Rules& rules,const terrain_materials& materials) {
  return sample_block_with_density(sampler.column(),y,rules,materials,[&] {return sampler.density(y);});
}

template <typename Rules>
inline uint16_t sample_block(const TerrainColumnSample& column, int32_t y, const Rules& rules) {
  return sample_block(column, y, rules, classify_materials(column, rules));
}

} // namespace octaryn::basegame::terrain
