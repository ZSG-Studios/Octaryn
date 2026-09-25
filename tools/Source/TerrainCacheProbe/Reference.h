#pragma once
#include "StreamSnapshot.h"
#include "TerrainColumn.h"
#include "TerrainVegetation.h"

namespace terrain_reference {
using namespace octaryn::basegame::terrain;
using namespace octaryn::client::world_presentation;

// Scalar terrain expressions retained independently of the cached path.
inline double noise(double x, double y, double z, std::uint32_t channel) {
  const auto ix=static_cast<std::int64_t>(std::floor(x));
  const auto iy=static_cast<std::int64_t>(std::floor(y));
  const auto iz=static_cast<std::int64_t>(std::floor(z));
  const double tx=fade(x-static_cast<double>(ix));
  const double ty=fade(y-static_cast<double>(iy));
  const double tz=fade(z-static_cast<double>(iz));
  const auto plane=[&](std::int64_t py) {
    return lerp(lerp(lattice(ix,py,iz,channel),lattice(ix+1,py,iz,channel),tx),
                lerp(lattice(ix,py,iz+1,channel),lattice(ix+1,py,iz+1,channel),tx),tz);
  };
  return lerp(plane(iy),plane(iy+1),ty);
}

inline double density(const TerrainColumnSample& column, std::int32_t y) {
  const std::int32_t depth=column.terrain_height-y;
  if(depth<=8 || y<WorldMinY+4)return 1;
  const double x=static_cast<double>(column.world_x),z=static_cast<double>(column.world_z);
  const double opening=ramp(8,24,depth)*ramp(WorldMinY+4,WorldMinY+20,y);
  const double chamber=noise(x*.022,y*.031,z*.022,307);
  const double a=noise(x*.036,y*.027,z*.036,353);
  const double b=noise(x*.031,y*.033,z*.031,401);
  return std::min(.64-chamber*opening,std::max(std::abs(a),std::abs(b))-.085*opening);
}

struct Materials {
  int water_height{30};
  std::uint16_t water_block{14},sand_block{3},grass_block{1},dirt_block{2},stone_block{5},snow_block{4};
};
inline std::uint16_t block(const TerrainColumnSample& column,int y,const terrain_materials& fill) {
  constexpr Materials rules{};
  if(y<WorldMinY || y>=WorldMaxYExclusive)return 0;
  if(y>column.terrain_height)return y<rules.water_height?rules.water_block:0;
  if(y==column.terrain_height)return fill.surface_block;
  if(y<WorldMinY+4)return rules.stone_block;
  if(density(column,y)<0)return 0;
  return column.terrain_height-y<=4?fill.fill_block:rules.stone_block;
}
inline std::size_t index(int x,int y,int z) {
  return static_cast<std::size_t>(x+32*((y-StreamWorldMinY)+StreamWorldHeight*z));
}
inline StreamColumn generate(const SnapshotColumn& source,std::uint64_t epoch) {
  StreamColumn result;
  result.x=source.x;result.z=source.z;result.epoch=epoch;result.revision=source.revision;
  result.blocks.resize(32*StreamWorldHeight*32);
  for(int z=0;z<32;++z)for(int x=0;x<32;++x) {
    const auto column=sample_column(source.x*32+x,source.z*32+z);
    const auto fill=classify_materials(column,Materials{});
    for(int y=StreamWorldMinY;y<StreamWorldMinY+StreamWorldHeight;++y) {
      const auto terrain=block(column,y,fill);
      result.blocks[index(x,y,z)]=sample_vegetation(column.world_x,y,column.world_z,
          terrain,Materials{},sample_column);
    }
  }
  for(const auto& edit:source.edits)
    result.blocks[index(edit.x-source.x*32,edit.y,edit.z-source.z*32)]=edit.block;
  result.blocks.compact();
  return result;
}
}
