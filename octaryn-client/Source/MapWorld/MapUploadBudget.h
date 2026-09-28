#pragma once
#include <algorithm>
#include <cstdint>
namespace octaryn::client::rendering {
inline constexpr std::uint64_t map_upload_chunk_bytes=256*1024;
inline constexpr std::uint64_t map_upload_frame_bytes=2*1024*1024;
inline std::uint64_t map_buffer_upload_bytes(std::uint64_t remaining,std::uint64_t budget) {
  return std::min({remaining,budget&~std::uint64_t(3),map_upload_chunk_bytes});
}
inline std::uint64_t map_texture_upload_rows(std::uint64_t remaining,std::uint64_t pitch,std::uint64_t budget) {
  if(!pitch)return 0;
  const auto aligned_budget=std::min(budget,map_upload_chunk_bytes)&~std::uint64_t(511);
  return std::min(remaining,aligned_budget/pitch);
}
}
