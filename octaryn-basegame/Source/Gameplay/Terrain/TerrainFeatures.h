#pragma once
#include "TerrainColumn.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace octaryn::basegame::terrain {
struct FeatureRules {
  std::uint16_t log{},leaves{},bush{};
  std::array<std::uint16_t,4> flowers{};
};
struct FeatureColumn {
  std::int32_t world_x{},world_z{},local_x{},local_z{};
  std::int32_t local_width{ChunkWidth},local_depth{ChunkDepth};
  std::int32_t decoration_y{};
  bool is_lowland{},has_grass_surface{};
};

// Original features.cpp at 3557cbf: caller supplies original plant noise in
// [-1,1], content IDs, and an already classified column. No noise or IDs invented
// here. Emission order is observable when adjacent canopies overlap.
template<class Emit>
void emit_features(const FeatureColumn& column,float plant_noise,
                   const FeatureRules& rules,Emit&& emit) {
  if(!column.is_lowland || !column.has_grass_surface || !std::isfinite(plant_noise) ||
     plant_noise < -1.0f || plant_noise > 1.0f) return;
  const auto put=[&](std::int64_t x,std::int64_t y,std::int64_t z,std::uint16_t block) {
    if(x<std::numeric_limits<std::int32_t>::min() || x>std::numeric_limits<std::int32_t>::max() ||
       z<std::numeric_limits<std::int32_t>::min() || z>std::numeric_limits<std::int32_t>::max() ||
       y<WorldMinY || y>=WorldMaxYExclusive) return;
    emit(static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),static_cast<std::int32_t>(z),block);
  };
  const float plant=plant_noise*0.5f+0.5f;
  if(plant>0.8f && column.local_x>2 && column.local_x<column.local_width-2 &&
     column.local_z>2 && column.local_z<column.local_depth-2) {
    const int log_height=static_cast<int>(3.0f+plant*2.0f);
    for(int dy=0;dy<log_height;++dy)
      put(column.world_x,std::int64_t(column.decoration_y)+dy+1,column.world_z,rules.log);
    for(int dx=-1;dx<=1;++dx)
      for(int dz=-1;dz<=1;++dz)
        for(int dy=0;dy<2;++dy)
          if(dx || dz || dy)
            put(std::int64_t(column.world_x)+dx,std::int64_t(column.decoration_y)+log_height+dy,
                std::int64_t(column.world_z)+dz,rules.leaves);
    return;
  }
  if(plant>0.55f) {
    put(column.world_x,std::int64_t(column.decoration_y)+1,column.world_z,rules.bush);
    return;
  }
  if(plant>0.52f) {
    const auto index=static_cast<std::size_t>(std::max(static_cast<int>(plant*1000.0f)%4,0));
    put(column.world_x,std::int64_t(column.decoration_y)+1,column.world_z,rules.flowers[index]);
  }
}

// Exact ordered bulk contract: anchors run localX outer/localZ inner; each
// anchor emits its terrain, then emit_features. TerrainEmitted distinguishes a
// sparse absent write from an explicit air write (e.g. a cave in a new owner).
// Only the queried column's terrain can write this cell; only same-chunk anchors
// within one block can emit a feature here. Queries must provide canonical 32x32
// column metadata and stable noise. Apply saved overrides AFTER this function.
template<class GetColumn,class GetNoise>
std::uint16_t sample_feature_overlay(std::int32_t x,std::int32_t y,std::int32_t z,
    std::uint16_t terrain_block,bool terrain_emitted,const FeatureRules& rules,
    GetColumn&& get_column,GetNoise&& get_noise) {
  if(y<WorldMinY || y>=WorldMaxYExclusive) return AirBlock;
  std::uint16_t result=terrain_block;
  const std::int64_t origin_x=std::int64_t(x)-floor_mod(x,ChunkWidth);
  const std::int64_t origin_z=std::int64_t(z)-floor_mod(z,ChunkDepth);
  for(std::int64_t ax=std::max(origin_x,std::int64_t(x)-1);
      ax<=std::min(origin_x+ChunkWidth-1,std::int64_t(x)+1);++ax) {
    for(std::int64_t az=std::max(origin_z,std::int64_t(z)-1);
        az<=std::min(origin_z+ChunkDepth-1,std::int64_t(z)+1);++az) {
      const auto anchor_x=static_cast<std::int32_t>(ax),anchor_z=static_cast<std::int32_t>(az);
      if(anchor_x==x && anchor_z==z && terrain_emitted) result=terrain_block;
      emit_features(get_column(anchor_x,anchor_z),get_noise(anchor_x,anchor_z),rules,
          [&](std::int32_t fx,std::int32_t fy,std::int32_t fz,std::uint16_t block) {
            if(fx==x && fy==y && fz==z) result=block;
          });
    }
  }
  return result;
}
}
