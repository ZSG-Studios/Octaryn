#pragma once
#include "ColumnBlocks.h"
#include "ColumnOrigin.h"
#include <array>
#include <cassert>
#include <memory>

namespace octaryn::client::world_presentation {
using ColumnOrigins = std::array<std::shared_ptr<const ColumnOrigin>,8>;
struct ColumnHalo {
  int min_y{},height{};
  ColumnBlocks blocks;
  ColumnOrigins neighbors;
};
constexpr std::size_t column_neighbor_index(int dx,int dz) {
  assert(dx>=-1 && dx<=1 && dz>=-1 && dz<=1 && (dx || dz));
  const int index=(dz+1)*3+dx+1;
  return static_cast<std::size_t>(index-(index>4?1:0));
}
// Border rows first/last, then two endpoints per interior row; Y is contiguous.
constexpr std::size_t column_halo_index(int x,int local_y,int z,int height) {
  assert(x>=-1 && x<=32 && z>=-1 && z<=32 && (x==-1 || x==32 || z==-1 || z==32));
  assert(height>0 && local_y>=0 && local_y<height);
  const int slot=z==-1?x+1:z==32?98+x+1:34+z*2+(x==32?1:0);
  return static_cast<std::size_t>(slot)*static_cast<std::size_t>(height)+static_cast<std::size_t>(local_y);
}
}
