#pragma once
#include "PredictedBlocks.h"
#include "WorldStream.h"

namespace octaryn::client::world_presentation {
inline bool predicted_block_index(const StreamColumn& source,int x,int y,int z,std::size_t& index) {
  if(y<source.min_y || y>=source.min_y+source.height)return false;
  const auto lx=x-source.x*32,lz=z-source.z*32;
  if(lx<0 || lx>=32 || lz<0 || lz>=32)return false;
  index=static_cast<std::size_t>(lx)+32u*(static_cast<std::size_t>(y-source.min_y)+
      static_cast<std::size_t>(source.height)*static_cast<std::size_t>(lz));
  return index<source.blocks.size();
}
inline StreamColumn compose_predicted_column(const StreamColumn& base,const PredictedBlocks& predictions) {
  auto result=base;
  for(const auto& edit:predictions.edits()) {
    std::size_t index{};
    if(predicted_block_index(result,edit.x,edit.y,edit.z,index))result.blocks[index]=edit.block;
  }
  return result;
}
}
