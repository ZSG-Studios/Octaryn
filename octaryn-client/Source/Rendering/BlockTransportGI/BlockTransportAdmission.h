#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace octaryn::client::rendering {
inline constexpr unsigned BlockAdmissionRadius=32;
inline constexpr unsigned BlockAdmissionColumns=9;
inline constexpr unsigned BlockAdmissionFaces=256;
struct BlockAdmissionColumn {
  std::int32_t x{},z{};
  std::uint32_t faces{},offset{};
  std::uint64_t sweeps{};
};
struct BlockAdmissionRange {std::uint32_t offset{},count{};};
struct BlockAdmissionState {
  std::array<BlockAdmissionColumn,BlockAdmissionColumns> columns{},pending_columns{};
  std::array<BlockAdmissionRange,BlockAdmissionColumns> ranges{};
  std::array<std::int32_t,4> minimum{},maximum{},contributor_minimum{},contributor_maximum{};
  std::uint64_t generation{},pending_generation{};
  unsigned count{},pending_count{};
};
inline bool block_admission_bounds(const std::array<float,4>& origin,
    std::array<std::int32_t,4>& minimum,std::array<std::int32_t,4>& maximum) {
  minimum={};maximum={};
  for(unsigned axis=0;axis<3;++axis) {
    const auto center=std::floor(double(origin[axis]));
    if(!std::isfinite(center) || center<double(INT32_MIN)+BlockAdmissionRadius ||
        center>double(INT32_MAX)-BlockAdmissionRadius)return false;
    minimum[axis]=static_cast<std::int32_t>(center-BlockAdmissionRadius);
    maximum[axis]=static_cast<std::int32_t>(center+BlockAdmissionRadius);
  }
  return true;
}
constexpr std::int32_t block_admission_column(std::int32_t block) {
  const auto value=std::int64_t(block);
  return static_cast<std::int32_t>((value-(value<0?31:0))/32);
}
inline BlockAdmissionRange block_admission_advance(BlockAdmissionColumn& column,unsigned budget) {
  if(!column.faces || !budget)return {};
  if(column.offset>=column.faces)column.offset=0;
  const BlockAdmissionRange result{column.offset,std::min(budget,column.faces-column.offset)};
  column.offset+=result.count;
  if(column.offset==column.faces) {column.offset=0;++column.sweeps;}
  return result;
}
}
