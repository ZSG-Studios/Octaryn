#pragma once
#include "SceneBounds.h"
#include <numeric>

namespace octaryn::scene_geometry {
class SpatialIndex {
  struct Node {Bounds bounds;std::uint32_t first{},count{},left{},right{};};
  std::vector<Node> nodes_;
  std::vector<std::uint32_t> indices_;
  std::uint32_t build(std::span<const Bounds> boxes,std::uint32_t first,std::uint32_t count) {
    const auto id=std::uint32_t(nodes_.size());Node node{};node.bounds=bounds::empty();node.first=first;node.count=count;
    for(unsigned i=first;i<first+count;++i)bounds::include(node.bounds,boxes[indices_[i]]);
    nodes_.push_back(node);
    if(count<=8)return id;
    unsigned axis{};
    for(unsigned i=1;i<3;++i)if(node.bounds[i+3]-node.bounds[i]>node.bounds[axis+3]-node.bounds[axis])axis=i;
    const auto middle=first+count/2;
    std::nth_element(indices_.begin()+first,indices_.begin()+middle,indices_.begin()+first+count,[&](auto a,auto b) {
      return double(boxes[a][axis])+boxes[a][axis+3]<double(boxes[b][axis])+boxes[b][axis+3];
    });
    nodes_[id].left=build(boxes,first,middle-first);
    nodes_[id].right=build(boxes,middle,first+count-middle);nodes_[id].count=0;
    return id;
  }
public:
  void reset(std::span<const Bounds> boxes) {
    nodes_.clear();indices_.resize(boxes.size());std::iota(indices_.begin(),indices_.end(),0);
    if(!boxes.empty())build(boxes,0,std::uint32_t(boxes.size()));
  }
  template<class Visitor> void query(const Bounds& box,Visitor&& visitor) const {
    if(nodes_.empty())return;
    std::vector<std::uint32_t> stack{0};
    while(!stack.empty()) {
      const auto& node=nodes_[stack.back()];stack.pop_back();
      if(!bounds::overlap(node.bounds,box))continue;
      if(node.count)for(unsigned i=node.first;i<node.first+node.count;++i)visitor(indices_[i]);
      else {stack.push_back(node.left);stack.push_back(node.right);}
    }
  }
  template<class Visitor> void query(const Bounds& box,const std::array<float,16>& transform,Visitor&& visitor) const {
    if(nodes_.empty())return;
    std::vector<std::uint32_t> stack{0};
    while(!stack.empty()) {
      const auto& node=nodes_[stack.back()];stack.pop_back();
      if(!bounds::overlap(bounds::transform(node.bounds,transform),box))continue;
      if(node.count)for(unsigned i=node.first;i<node.first+node.count;++i)visitor(indices_[i]);
      else {stack.push_back(node.left);stack.push_back(node.right);}
    }
  }
};
}
