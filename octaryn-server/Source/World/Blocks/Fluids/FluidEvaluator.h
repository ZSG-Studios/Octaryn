#pragma once
#include "FluidRules.h"
#include <functional>

namespace octaryn::server::world::blocks {
using FluidRead=std::function<bool(const BlockPosition&,std::uint16_t&)>;
// Pure proposal: true means next is valid (possibly unchanged); false means an
// unavailable required sample or invalid position. On false, next is untouched.
// No writes, scheduling, clock, persistence, worker or client state is owned here.
// Caller supplies validated rules and a stable read view for the entire call
// (including slope lookahead), not a world that can change between callbacks.
// Above/below the signed world are known empty, but donors at WorldMinY cannot
// fall outside the world. In-range unavailable reads are never treated as air.
// This intentionally defers unavailable neighbor samples that the old loader
// ignored as air; original evaluator parity applies to complete stable samples.
bool evaluate_fluid(BlockPosition position,const FluidRules& rules,const FluidRead& read,std::uint16_t& next);
}
