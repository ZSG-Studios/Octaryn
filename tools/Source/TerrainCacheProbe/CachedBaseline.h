#pragma once
#include "Reference.h"
#include "TerrainDensity.h"

namespace terrain_cached_baseline {
using namespace octaryn::basegame::terrain;
using namespace octaryn::client::world_presentation;
// Previous production generation: cached full density, per-cell COW checks,
// every vertical level, then the same vegetation, authoritative edits and codec.
inline std::uint16_t block(const CaveColumnSampler& caves,int y,const terrain_materials& fill) {
  constexpr terrain_reference::Materials rules{};
  const auto& column=caves.column();
  if(y<WorldMinY || y>=WorldMaxYExclusive)return AirBlock;
  if(y>column.terrain_height)return y<rules.water_height?rules.water_block:AirBlock;
  if(y==column.terrain_height)return fill.surface_block;
  if(y<WorldMinY+4)return rules.stone_block;
  if(caves.density(y)<0)return AirBlock;
  return column.terrain_height-y<=4?fill.fill_block:rules.stone_block;
}
inline StreamColumn generate(const SnapshotColumn& source,std::uint64_t epoch) {
  constexpr terrain_reference::Materials rules{};
  using terrain_reference::index;
  StreamColumn result;
  result.x=source.x;result.z=source.z;result.epoch=epoch;result.revision=source.revision;
  result.authoritative_revision=source.authoritative_revision;
  result.blocks.resize(32*StreamWorldHeight*32);
  for(int z=0;z<32;++z)for(int x=0;x<32;++x) {
    const auto column=sample_column(source.x*32+x,source.z*32+z);
    const auto fill=classify_materials(column,rules);
    const CaveColumnSampler caves(column);
    for(int y=StreamWorldMinY;y<StreamWorldMinY+StreamWorldHeight;++y)
      result.blocks[index(x,y,z)]=block(caves,y,fill);
  }
  for(int z=-VegetationRadius;z<32+VegetationRadius;++z)
    for(int x=-VegetationRadius;x<32+VegetationRadius;++x)
      emit_vegetation(source.x*32+x,source.z*32+z,rules,sample_column,
          [&](int wx,int y,int wz,std::uint16_t block) {
            const int lx=wx-source.x*32,lz=wz-source.z*32;
            if(lx<0 || lx>=32 || lz<0 || lz>=32)return;
            const auto cell=index(lx,y,lz);
            const std::uint16_t current=result.blocks[cell];
            if(current==AirBlock || current==LogBlock || current==LeavesBlock ||
                current==BushBlock || (current>=10 && current<=13))
              result.blocks[cell]=merge_vegetation(current,block);
          });
  for(const auto& edit:source.edits)
    result.blocks[index(edit.x-source.x*32,edit.y,edit.z-source.z*32)]=edit.block;
  result.blocks.compact();return result;
}
}
