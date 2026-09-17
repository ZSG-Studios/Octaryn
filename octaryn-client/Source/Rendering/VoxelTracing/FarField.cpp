#include "FarField.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering {
unsigned far_field_width(unsigned level) {
  if(level>2)throw std::invalid_argument("Far-field level must be 0, 1 or 2");
  return 4u<<(2*level);
}
FarFieldKey far_field_key(std::int32_t x,std::int32_t y,std::int32_t z,unsigned level) {
  const auto width=std::int32_t(far_field_width(level));
  const auto floor=[&](std::int32_t value){return value/width-(value%width<0?1:0);};
  return {floor(x),floor(y),floor(z),level};
}
FarFieldNode far_field_aggregate(std::span<const FarFieldNode,64> children) {
  FarFieldNode result;
  for(unsigned i=0;i<64;++i) {
    const auto& child=children[i];const auto bit=std::uint64_t{1}<<i;
    if(child.complete())result.known|=bit;
    if(child.occupied)result.occupied|=bit;
    if(child.complete() && child.uniform==~std::uint64_t{} &&
        std::all_of(child.material.begin(),child.material.end(),[&](auto id){return id==child.material[0];})) {
      result.uniform|=bit;result.material[i]=child.material[0];
    }
    result.material_features|=child.material_features;
    result.authority_revision=std::max(result.authority_revision,child.authority_revision);
    result.geometry_epoch=std::max(result.geometry_epoch,child.geometry_epoch);
  }
  return result;
}
const FarFieldNode* FarFieldCache::find(FarFieldKey key) const {
  const auto found=nodes_.find(key);return found==nodes_.end()?nullptr:&found->second.node;
}
bool FarFieldCache::publish(FarFieldKey key,FarFieldNode node,std::uint64_t ticket) {
  if(ticket!=ticket_ || !capacity_ || key.level>2)return false;
  node.geometry_epoch=ticket_;
  if(const auto found=nodes_.find(key);found!=nodes_.end()) {found->second.node=std::move(node);return true;}
  if(nodes_.size()==capacity_) {nodes_.erase(order_.front());order_.pop_front();}
  order_.push_back(key);nodes_.emplace(key,Entry{std::move(node),std::prev(order_.end())});return true;
}
void FarFieldCache::invalidate(std::int32_t x,std::int32_t y,std::int32_t z) {
  ++ticket_;
  for(unsigned level=0;level<3;++level) {
    const auto found=nodes_.find(far_field_key(x,y,z,level));
    if(found!=nodes_.end()) {order_.erase(found->second.order);nodes_.erase(found);}
  }
}
void FarFieldCache::reset(std::uint64_t world_epoch) {
  if(world_epoch==world_epoch_)return;
  world_epoch_=world_epoch;++ticket_;nodes_.clear();order_.clear();
}
void FarFieldCache::invalidate_column(std::int32_t column_x,std::int32_t column_z) {
  ++ticket_;
  const auto x=std::int64_t(column_x)*32,z=std::int64_t(column_z)*32;
  for(auto it=nodes_.begin();it!=nodes_.end();) {
    const auto width=far_field_width(it->first.level);
    const auto nx=std::int64_t(it->first.x)*width,nz=std::int64_t(it->first.z)*width;
    if(nx<x+32 && nx+width>x && nz<z+32 && nz+width>z) {
      order_.erase(it->second.order);it=nodes_.erase(it);
    } else ++it;
  }
}
} // namespace octaryn::client::rendering
