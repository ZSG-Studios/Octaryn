#pragma once
#include <algorithm>
#include <cstdint>

namespace octaryn::client::rendering {
// Integer policy shared by runtime admission and the boundary fixture.
inline std::uint64_t tile_gpu_headroom(std::uint64_t budget,std::uint64_t usage,std::uint64_t admitted) {
  const auto ceiling=budget/10*7+(budget%10)*7/10;
  if(usage>=ceiling || admitted>=ceiling-usage)return 0;
  return ceiling-usage-admitted;
}
inline bool tile_gpu_admits(std::uint64_t bytes,std::uint64_t budget,std::uint64_t usage,std::uint64_t admitted,
    std::uint64_t reserved=0) {
  const auto headroom=tile_gpu_headroom(budget,usage,admitted);
  return reserved<=headroom && bytes<=headroom-reserved;
}
}
