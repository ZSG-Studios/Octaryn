#pragma once
#include <array>
#include <cstdint>
#include <memory_resource>
#include <unordered_map>

namespace octaryn::client::rendering {
struct ItemPreviousPose {
  std::uint64_t generation{},frame{},source_tick{},received_ns{};
  std::array<float,3> position;
};
using ItemHistory=std::pmr::unordered_map<std::uint64_t,ItemPreviousPose>;
inline bool prewarm_item_history(ItemHistory& history,unsigned count,unsigned maximum) {
  if(!history.empty() || !count || count>maximum || maximum>10000)return false;
  // A frame inserts its new generation before removing absent prior entities.
  history.reserve(2*maximum);
  for(unsigned i=0;i<2*count;++i)history.emplace(i,ItemPreviousPose{});
  history.clear();return true;
}
}
