#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace octaryn::client::voxel_tracing {
inline constexpr unsigned ChunkWidth=32,LeafWidth=4,MacroWidth=16;
inline constexpr unsigned LeafCount=512,MacroCount=8,MaterialWordCount=16384;
struct ChunkKey {
  std::int32_t x{},y{},z{};
  auto operator<=>(const ChunkKey&) const=default;
};
constexpr std::int64_t floor_div(std::int64_t value,std::int64_t divisor) {
  return value/divisor-(value%divisor<0);
}
constexpr unsigned voxel_index(unsigned x,unsigned y,unsigned z) {return x+32*(y+32*z);}
constexpr unsigned leaf_index(unsigned x,unsigned y,unsigned z) {return x/4+8*(y/4+8*(z/4));}
constexpr unsigned leaf_bit(unsigned x,unsigned y,unsigned z) {return x%4+4*(y%4+4*(z%4));}
constexpr unsigned macro_index(unsigned x,unsigned y,unsigned z) {return x/16+2*(y/16+2*(z/16));}
constexpr unsigned macro_bit(unsigned x,unsigned y,unsigned z) {return (x/4)%4+4*((y/4)%4+4*((z/4)%4));}

// GPU element offsets: uint64 leaves/macros, uint32 packed material words.
// Portable Slang readers may view each uint64 mask/epoch as uint2(low,high).
struct ChunkHeader {
  std::array<std::int32_t,3> coordinate{};
  std::uint32_t macro_mask{};
  std::uint64_t geometry_epoch{};
  std::uint32_t leaf_offset{},macro_offset{},material_offset{};
  std::uint32_t min_local_y{},max_local_y{},flags{};
  bool operator==(const ChunkHeader&) const=default;
};
inline constexpr std::uint32_t ChunkKnown=1;
static_assert(sizeof(ChunkHeader)==48 && offsetof(ChunkHeader,geometry_epoch)==16 &&
  offsetof(ChunkHeader,material_offset)==32 && offsetof(ChunkHeader,flags)==44);
struct ChunkPayload {
  ChunkHeader header;
  std::array<std::uint64_t,LeafCount> leaves{};
  std::array<std::uint64_t,MacroCount> macros{};
  // Two block IDs per word: even voxel in low 16 bits, odd voxel in high 16.
  std::array<std::uint32_t,MaterialWordCount> materials{};
};
enum class Residency {Unknown,Air,Occupied};
struct VoxelSample {
  Residency residency{Residency::Unknown};
  std::uint16_t block{};
  std::uint64_t geometry_epoch{};
};
// Subtract integers before conversion. Refuse a frame too distant for exact floats.
bool relative_chunk_origin(ChunkKey,const std::array<std::int64_t,3>& anchor,std::array<float,3>& result);
}
