#include "SceneRootPages.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
SceneRootPages::SceneRootPages(std::uint32_t slots):slabs_(slots) {
  if(!slots || slots>8192)throw std::invalid_argument("invalid packed root slab capacity");
  for(auto& slab:slabs_)slab.push_back({0,page_bytes,0});
}
SceneRootSpan SceneRootPages::reserve(std::uint32_t bytes) {
  if(!bytes || bytes>page_bytes || generation_==UINT32_MAX)return {};
  bytes=(bytes+15)&~15u;
  std::uint32_t chosen=invalid_id,index{},smallest=page_bytes+1;
  for(unsigned slot=0;slot<slabs_.size();++slot)for(unsigned i=0;i<slabs_[slot].size();++i) {
    const auto& range=slabs_[slot][i];
    if(!range.generation && range.bytes>=bytes && range.bytes<smallest) {chosen=slot;index=i;smallest=range.bytes;}
  }
  if(chosen==invalid_id)return {};
  auto& slab=slabs_[chosen];const auto offset=slab[index].offset;
  if(smallest>bytes)slab.insert(slab.begin()+index+1,{offset+bytes,smallest-bytes,0});
  const auto generation=++generation_;slab[index]={offset,bytes,generation};bytes_+=bytes;
  return {chosen,offset,bytes,generation};
}
bool SceneRootPages::valid(SceneRootSpan span) const {
  if(!span || span.slot>=slabs_.size())return false;
  for(const auto& range:slabs_[span.slot])if(range.generation==span.generation &&
      range.offset==span.offset && range.bytes==span.bytes)return true;
  return false;
}
bool SceneRootPages::release(SceneRootSpan span,std::uint64_t required,std::uint64_t completed) {
  if(required>completed || !valid(span))return false;
  auto& slab=slabs_[span.slot];
  for(auto& range:slab)if(range.generation==span.generation) {range.generation=0;bytes_-=range.bytes;break;}
  for(unsigned i=1;i<slab.size();) {
    if(!slab[i-1].generation && !slab[i].generation) {
      slab[i-1].bytes+=slab[i].bytes;slab.erase(slab.begin()+i);
    } else ++i;
  }
  return true;
}
std::uint32_t SceneRootPages::required_slots(std::span<const std::uint32_t> sizes) {
  if(sizes.empty())return 0;
  std::vector<std::uint32_t> free;
  for(auto size:sizes) {
    if(!size || size>page_bytes)return invalid_id;
    size=(size+15)&~15u;auto found=free.end();
    for(auto i=free.begin();i!=free.end();++i)if(*i>=size && (found==free.end() || *i<*found))found=i;
    if(found==free.end())free.push_back(page_bytes-size);else *found-=size;
  }
  return unsigned(free.size());
}
std::vector<std::uint32_t> geometry_page_payload_bytes(const GeometryAsset& asset) {
  std::vector<std::uint32_t> sizes(asset.pages.size());
  for(const auto& cluster:asset.clusters) {
    const auto stride=(cluster.flags&geometry_position_only)?12u:80u;
    const auto vertices=std::uint64_t(cluster.vertex_offset)+std::uint64_t(cluster.vertex_count)*stride;
    const auto triangles=std::uint64_t(cluster.triangle_offset)+std::uint64_t(cluster.triangle_count)*4;
    if(cluster.page>=sizes.size() || std::max(vertices,triangles)>page_bytes)
      throw std::invalid_argument("packed root cluster payload exceeds its page");
    sizes[cluster.page]=std::max(sizes[cluster.page],unsigned((std::max(vertices,triangles)+15)&~std::uint64_t(15)));
  }
  return sizes;
}
}
