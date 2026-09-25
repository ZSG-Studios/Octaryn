#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace octaryn::client::rendering {
inline constexpr std::uint32_t BlockTransportCapacity=65536;
inline constexpr std::uint32_t BlockTransportLinks=16;
inline constexpr std::uint32_t BlockTransportRows=2048;
inline constexpr std::uint32_t BlockTransportSelectGroup=256;
inline constexpr std::uint32_t BlockTransportDirectCalls=2;
inline constexpr std::uint32_t BlockTransportProbes=16;
struct BlockSurfaceKey {
  std::int32_t x{},y{},z{};
  std::uint32_t direction{};
  friend bool operator==(const BlockSurfaceKey&,const BlockSurfaceKey&)=default;
};
struct alignas(16) BlockTransportSurface {
  BlockSurfaceKey key;
  std::array<float,4> albedo{};
  std::array<std::uint32_t,4> state{};
  std::array<std::uint32_t,4> extra{};
};
struct alignas(16) BlockTransportLink {
  std::uint32_t target_slot{},target_epoch{},target_generation{},terminal{};
};
struct alignas(16) BlockTransportCandidate {
  BlockSurfaceKey key;
  std::array<float,4> albedo{};
};
struct alignas(16) BlockTransportWorkRow {
  std::uint32_t slot{},generation{},epoch{},player_revision{};
};
static_assert(sizeof(BlockSurfaceKey)==16);
static_assert(sizeof(BlockTransportSurface)==64);
static_assert(sizeof(BlockTransportLink)==16);
static_assert(sizeof(BlockTransportCandidate)==32);
static_assert(sizeof(BlockTransportWorkRow)==16);

constexpr std::uint32_t block_transport_hash(std::uint32_t value) {
  value^=value>>16;value*=0x7feb352du;value^=value>>15;
  value*=0x846ca68bu;return value^(value>>16);
}
constexpr std::uint32_t block_surface_hash(BlockSurfaceKey key) {
  return block_transport_hash(std::uint32_t(key.x)^block_transport_hash(std::uint32_t(key.y))^
      block_transport_hash(std::uint32_t(key.z)+0x9e3779b9u)^block_transport_hash(key.direction+0x85ebca6bu));
}

constexpr bool block_transport_plant_tag(std::uint32_t tag) {
  const auto kind=tag&15u,layer=tag>>4;
  return kind>=6 && kind<=9 && layer<29 && (layer<17 || layer>23);
}
constexpr bool block_transport_valid_tag(std::uint32_t tag) {
  return tag<6 || block_transport_plant_tag(tag);
}
inline BlockSurfaceKey block_crossed_plant_key(std::array<std::int32_t,3> anchor,
    unsigned direction,unsigned layer,std::array<float,3> toward) {
  if(direction<6 || direction>=10 || layer>=29 || (layer>=17 && layer<=23) ||
      !std::isfinite(toward[0]) || !std::isfinite(toward[1]) || !std::isfinite(toward[2]))
    return {0,0,0,UINT32_MAX};
  const auto plane=(direction-6)/2;
  const float alignment=(plane==0?-toward[0]:toward[0])+toward[2];
  const auto tag=(6+plane*2+(alignment<0?1u:0u))|(layer<<4);
  return {anchor[0],anchor[1],anchor[2],tag};
}

inline BlockSurfaceKey block_surface_key(std::array<std::int32_t,3> anchor,
    unsigned direction,unsigned extent_u,unsigned extent_v,std::array<float,3> local) {
  if(direction>=6 || !extent_u || !extent_v || extent_u>32 || extent_v>32 ||
      !std::isfinite(local[0]) || !std::isfinite(local[1]) || !std::isfinite(local[2]))
    return {0,0,0,UINT32_MAX};
  const std::array<unsigned,3> size=direction<2?std::array<unsigned,3>{1,extent_v,extent_u}:
      direction<4?std::array<unsigned,3>{extent_u,1,extent_v}:std::array<unsigned,3>{extent_u,extent_v,1};
  std::array<std::int32_t,3> cell=anchor;
  for(unsigned axis=0;axis<3;++axis) {
    const auto value=std::int64_t(anchor[axis])+static_cast<std::int32_t>(
        std::clamp(std::floor(local[axis]),0.f,float(size[axis]-1)));
    if(value<std::numeric_limits<std::int32_t>::min() || value>std::numeric_limits<std::int32_t>::max())
      return {0,0,0,UINT32_MAX};
    cell[axis]=static_cast<std::int32_t>(value);
  }
  return {cell[0],cell[1],cell[2],direction};
}
}
