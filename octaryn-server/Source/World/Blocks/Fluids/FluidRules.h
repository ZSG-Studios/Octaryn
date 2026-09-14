#pragma once
#include "BlockStore.h"
#include <algorithm>
#include <array>
#include <vector>

namespace octaryn::server::world::blocks {
enum class FluidKind {None,Water,Lava};
// Supplied by the registered game module; level zero is the persistent source.
// Validate once before using the immutable configuration with the evaluator.
struct FluidRules {
  std::array<std::uint16_t,8> water{},lava{};
  std::uint16_t stone{};
  std::vector<std::uint16_t> replaceable,solid;
  FluidKind kind(std::uint16_t block) const {
    if(std::find(water.begin(),water.end(),block)!=water.end()) return FluidKind::Water;
    if(std::find(lava.begin(),lava.end(),block)!=lava.end()) return FluidKind::Lava;
    return FluidKind::None;
  }
  int level(std::uint16_t block) const {
    for(unsigned i=0;i<8;++i) if(water[i]==block || lava[i]==block) return static_cast<int>(i);
    return -1;
  }
  bool source(std::uint16_t block) const {return block==water[0] || block==lava[0];}
  bool is_solid(std::uint16_t block) const {return std::find(solid.begin(),solid.end(),block)!=solid.end();}
  bool is_replaceable(std::uint16_t block) const {return std::find(replaceable.begin(),replaceable.end(),block)!=replaceable.end();}
  std::uint16_t make(FluidKind kind,int level) const {
    const auto index=static_cast<std::size_t>(std::clamp(level,0,7));
    return kind==FluidKind::Water?water[index]:kind==FluidKind::Lava?lava[index]:AirBlock;
  }
};
bool validate_fluid_rules(const FluidRules& rules);
}
