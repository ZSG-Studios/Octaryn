#include "FarFieldGenerator.h"
#include "WorldStream.h"

namespace octaryn::client::rendering {
FarFieldBuild far_field_resident(FarFieldKey key,const world_presentation::StreamColumn& column,
    FarFieldMaterialFeatures features,void* context) {
  if(key.level || column.height<=0 || column.blocks.size()!=std::size_t(column.height)*32*32)return {};
  const auto x=std::int64_t(key.x)*4-std::int64_t(column.x)*32;
  const auto y=std::int64_t(key.y)*4-column.min_y;
  const auto z=std::int64_t(key.z)*4-std::int64_t(column.z)*32;
  if(x<0 || x+4>32 || y<0 || y+4>column.height || z<0 || z+4>32)return {};
  FarFieldNode node;node.known=node.uniform=~std::uint64_t{};node.authority_revision=column.authoritative_revision;
  for(unsigned dz=0;dz<4;++dz)for(unsigned dy=0;dy<4;++dy)for(unsigned dx=0;dx<4;++dx) {
    const auto index=dx+4*(dy+4*dz);
    const auto block=column.blocks[std::size_t(x+dx+32*(y+dy+std::int64_t(column.height)*(z+dz)))];
    node.material[index]=block;
    if(block) {
      node.occupied|=std::uint64_t{1}<<index;
      if(features)node.material_features|=features(block,context);
    }
  }
  return {FarFieldBuildState::Ready,std::move(node)};
}
} // namespace octaryn::client::rendering
