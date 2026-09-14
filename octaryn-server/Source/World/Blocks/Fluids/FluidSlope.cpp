// Port of Octaryn 3557cbf source/world/edit/water.cpp:189-351.
#include "FluidSampling.h"

namespace octaryn::server::world::blocks::fluid_detail {
bool Sampling::downward(BlockPosition p,FluidKind kind) {
  if(!available || kind==FluidKind::None || p.y<=WorldMinY) return false;
  const auto below=block(p,0,-1,0);
  return !(rules.kind(below)==kind && rules.source(below)) && accepts(below,kind);
}
bool Sampling::horizontal(BlockPosition p,FluidKind kind) {
  if(!available) return false;
  const auto current=block(p);
  return !(rules.kind(current)==kind && rules.source(current)) && accepts(current,kind);
}
int Sampling::slope_distance(BlockPosition p,FluidKind kind,int pass,int from_direction) {
  int lowest=1000;
  const int maximum=kind==FluidKind::Lava?2:4;
  for(int direction=0;available && direction<4;++direction) {
    if(direction==from_direction) continue;
    BlockPosition target{};
    if(!offset(p,dx[direction],0,dz[direction],target) || !horizontal(target,kind)) continue;
    if(downward(target,kind)) return pass;
    if(pass<maximum) lowest=std::min(lowest,slope_distance(target,kind,pass+1,direction^1));
  }
  return lowest;
}
bool Sampling::donor_spreads(BlockPosition donor,int target_direction,FluidKind kind) {
  if(!available || kind==FluidKind::None) return false;
  const auto current=block(donor);
  if(rules.kind(current)!=kind || rules.level(current)<0 || rules.level(current)>=7) return false;
  BlockPosition target{};
  if(!offset(donor,dx[target_direction],0,dz[target_direction],target) || !horizontal(target,kind)) return false;
  if(downward(donor,kind)) {
    int sources=0;
    for(int i=0;available && i<4;++i) {
      const auto neighbor=block(donor,dx[i],0,dz[i]);
      if(rules.kind(neighbor)==kind && rules.source(neighbor)) ++sources;
    }
    if(sources<3) return false;
  }
  int best=1000,target_distance=1000;
  for(int direction=0;available && direction<4;++direction) {
    BlockPosition candidate{};
    if(!offset(donor,dx[direction],0,dz[direction],candidate) || !horizontal(candidate,kind)) continue;
    const int distance=downward(candidate,kind)?0:slope_distance(candidate,kind,1,direction^1);
    best=std::min(best,distance);
    if(direction==target_direction) target_distance=distance;
  }
  // Equal unreachable distances intentionally allow all directions on flat ground.
  return available && target_distance==best;
}
}
