#pragma once
#include "FluidEvaluator.h"
#include <limits>

namespace octaryn::server::world::blocks::fluid_detail {
// Original source/world/direction.h: north/south/east/west, opposite=i^1.
inline constexpr int dx[4]={0,0,1,-1},dz[4]={1,-1,0,0};
struct Sampling {
  const FluidRules& rules;
  const FluidRead& read;
  bool available{true};
  bool offset(BlockPosition p,int x,int y,int z,BlockPosition& out) {
    const auto nx=std::int64_t(p.x)+x,nz=std::int64_t(p.z)+z;
    if(nx<std::numeric_limits<std::int32_t>::min() || nx>std::numeric_limits<std::int32_t>::max() ||
       nz<std::numeric_limits<std::int32_t>::min() || nz>std::numeric_limits<std::int32_t>::max()) {
      available=false;return false;
    }
    out={static_cast<std::int32_t>(nx),p.y+y,static_cast<std::int32_t>(nz)};return true;
  }
  std::uint16_t block(BlockPosition p,int x=0,int y=0,int z=0) {
    BlockPosition at{};
    if(!offset(p,x,y,z,at)) return AirBlock;
    if(at.y<WorldMinY || at.y>=WorldMaxYExclusive) return AirBlock;
    std::uint16_t result{};
    if(!read(at,result)) {available=false;return AirBlock;}
    return result;
  }
  bool accepts(std::uint16_t block,FluidKind kind) const {
    return block==AirBlock || rules.is_replaceable(block) || rules.kind(block)==kind;
  }
  bool downward(BlockPosition p,FluidKind kind);
  bool horizontal(BlockPosition p,FluidKind kind);
  int slope_distance(BlockPosition p,FluidKind kind,int pass,int from_direction);
  bool donor_spreads(BlockPosition donor,int target_direction,FluidKind kind);
};
}
