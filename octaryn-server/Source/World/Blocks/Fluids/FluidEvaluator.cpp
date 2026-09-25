// Port of Octaryn 3557cbf source/world/edit/water.cpp:367-545,636-680.
#include "FluidSampling.h"
#include <set>

namespace octaryn::server::world::blocks {
bool validate_fluid_rules(const FluidRules& rules) {
  std::set<std::uint16_t> levels;
  for(const auto& table:{rules.water,rules.lava}) for(const auto block:table)
    if(block==AirBlock || !levels.insert(block).second) return false;
  if(rules.stone==AirBlock || levels.contains(rules.stone) || !rules.is_solid(rules.stone)) return false;
  for(const auto* table:{&rules.replaceable,&rules.solid}) {
    if(table->size()>65535) return false;
    std::set<std::uint16_t> seen;
    for(const auto block:*table) if(block==AirBlock || levels.contains(block) || !seen.insert(block).second) return false;
  }
  return !rules.is_replaceable(rules.stone);
}
namespace {
struct Sample {
  std::uint16_t current{},above{},below{};
  std::array<std::uint16_t,4> neighbors{};
  std::array<bool,4> spreads{};
  bool support{};
};
bool incoming(const Sample& sample,const FluidRules& rules,FluidKind kind) {
  if(rules.kind(sample.above)==kind) return true;
  for(unsigned i=0;i<4;++i) if(sample.spreads[i] && rules.kind(sample.neighbors[i])==kind) return true;
  return false;
}
bool touches(const Sample& sample,const FluidRules& rules,FluidKind kind) {
  if(rules.kind(sample.above)==kind || rules.kind(sample.below)==kind) return true;
  for(const auto neighbor:sample.neighbors) if(rules.kind(neighbor)==kind) return true;
  return false;
}
std::uint16_t compute(const Sample& sample,const FluidRules& rules) {
  const auto current=rules.kind(sample.current);
  // Contact conversion precedes source preservation: lava sources also harden.
  if(current==FluidKind::Lava && touches(sample,rules,FluidKind::Water)) return rules.stone;
  const bool water=incoming(sample,rules,FluidKind::Water),lava=incoming(sample,rules,FluidKind::Lava);
  const auto kind=current!=FluidKind::None?current:water==lava?FluidKind::None:water?FluidKind::Water:FluidKind::Lava;
  if(kind==FluidKind::None || rules.source(sample.current)) return sample.current;
  int next_level=99,water_sources=0;
  for(unsigned i=0;i<4;++i) {
    const auto block=sample.neighbors[i];const int level=rules.level(block);
    if(!sample.spreads[i] || rules.kind(block)!=kind || level<0 || level>=7) continue;
    next_level=std::min(next_level,level+1);
    if(block==rules.water[0]) ++water_sources;
  }
  // Vegetation is replaced only by direct contact, before water-source creation.
  if(rules.is_replaceable(sample.current)) {
    if(rules.kind(sample.above)==kind) return rules.make(kind,1);
    return next_level<=7?rules.make(kind,next_level):sample.current;
  }
  if(kind==FluidKind::Water && water_sources>=2 && sample.support) return rules.water[0];
  if(rules.kind(sample.above)==kind) return rules.make(kind,1);
  return next_level<=7?rules.make(kind,next_level):AirBlock;
}
}
bool evaluate_fluid(BlockPosition position,const FluidRules& rules,const FluidRead& read,std::uint16_t& next) {
  if(!read || position.y<WorldMinY || position.y>=WorldMaxYExclusive) return false;
  fluid_detail::Sampling sampling{rules,read};
  Sample sample;sample.current=sampling.block(position);
  if(!sampling.available) return false;
  if(sample.current!=AirBlock && !rules.is_replaceable(sample.current) && rules.kind(sample.current)==FluidKind::None) {
    next=sample.current;return true;
  }
  sample.above=sampling.block(position,0,1,0);sample.below=sampling.block(position,0,-1,0);
  sample.support=rules.is_solid(sample.below) || sample.below==rules.water[0];
  for(unsigned i=0;sampling.available && i<4;++i) {
    BlockPosition neighbor{};
    if(!sampling.offset(position,fluid_detail::dx[i],0,fluid_detail::dz[i],neighbor)) break;
    sample.neighbors[i]=sampling.block(neighbor);
    sample.spreads[i]=sampling.donor_spreads(neighbor,static_cast<int>(i)^1,rules.kind(sample.neighbors[i]));
  }
  if(!sampling.available) return false;
  next=compute(sample,rules);return true;
}
}
