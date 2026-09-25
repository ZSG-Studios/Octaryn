#pragma once
#include "FluidEvaluator.h"
#include <stdexcept>
#include <string>
namespace fluid_probe {
using namespace octaryn::server::world::blocks;
inline unsigned checks{};
inline void require(bool ok,const std::string& message) {
  ++checks;if(!ok) throw std::runtime_error(message);
}
void check_apply(const FluidRules& rules);
void check_scheduler(const FluidRules& rules);
void check_fluid_abi(const FluidRules& rules);
}
