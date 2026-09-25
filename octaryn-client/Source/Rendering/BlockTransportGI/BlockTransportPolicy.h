#pragma once
#include "BlockTransportTypes.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace octaryn::client::rendering {
inline constexpr unsigned BlockContributorMargin=3*32+2;
inline constexpr unsigned BlockContributorOccupancy=BlockTransportCapacity*3/4;
inline void block_contributor_bounds(const std::array<std::int32_t,4>& minimum,
    const std::array<std::int32_t,4>& maximum,std::array<std::int32_t,4>& expanded_minimum,
    std::array<std::int32_t,4>& expanded_maximum) {
  expanded_minimum={};expanded_maximum={};
  for(unsigned axis=0;axis<3;++axis) {
    expanded_minimum[axis]=static_cast<std::int32_t>(std::max(std::int64_t(INT32_MIN),
        std::int64_t(minimum[axis])-BlockContributorMargin));
    expanded_maximum[axis]=static_cast<std::int32_t>(std::min(std::int64_t(INT32_MAX),
        std::int64_t(maximum[axis])+BlockContributorMargin));
  }
}
constexpr unsigned block_contributor_limit(bool mandatory_ready,unsigned pinned) {
  return mandatory_ready?std::min(BlockTransportCapacity,std::max(BlockContributorOccupancy,pinned)):0;
}
constexpr bool block_contributor_phase_ready(std::uint64_t measured_frame,std::uint64_t first_frame) {
  return first_frame!=UINT64_MAX && measured_frame>=first_frame;
}
inline bool block_mandatory_clean_sweep(std::uint64_t sweeps,std::uint32_t failures,
    std::uint64_t& checked_sweeps,std::uint32_t& checked_failures,bool clean) {
  if(sweeps>checked_sweeps) {
    clean=failures==checked_failures;checked_sweeps=sweeps;checked_failures=failures;
  }
  return clean && failures==checked_failures;
}
}
