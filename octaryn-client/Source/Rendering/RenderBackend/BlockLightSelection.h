#pragma once
#include "LocalLight.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace octaryn::client::rendering {
class BlockLightSelection {
  struct Candidate {
    float distance,influence;
    std::uint64_t order;
    const WorldLocalLight* light;
  };
  std::array<float,3> center_;
  std::size_t budget_;
  std::uint64_t next_order_{};
  std::vector<Candidate> selected_;
  static bool before(const Candidate& a,const Candidate& b) {
    if(a.influence!=b.influence)return a.influence<b.influence;
    if(a.distance!=b.distance)return a.distance<b.distance;
    return a.order<b.order;
  }
  Candidate candidate(const WorldLocalLight& light) {
    float squared{};
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=light.position_range[axis]-center_[axis];squared+=delta*delta;
    }
    return {squared,std::max(0.f,std::sqrt(squared)-light.position_range[3]-8.f),next_order_++,&light};
  }
public:
  BlockLightSelection(std::array<float,3> center,std::size_t explicit_count):center_(center),
      budget_(std::min<std::size_t>(4096,65536-std::min<std::size_t>(explicit_count,65536))) {
    selected_.reserve(budget_);
  }
  // Every resident emitter is eligible. Source visitation order breaks exact ties.
  void add(const WorldLocalLight& light) {
    if(!budget_)return;
    const auto value=candidate(light);
    if(selected_.size()<budget_) {
      selected_.push_back(value);std::push_heap(selected_.begin(),selected_.end(),before);
    } else if(before(value,selected_.front())) {
      std::pop_heap(selected_.begin(),selected_.end(),before);selected_.back()=value;
      std::push_heap(selected_.begin(),selected_.end(),before);
    }
  }
  std::size_t selected_count() const {return selected_.size();}
  // Borrowed source pointers remain valid through this owner-thread selection.
  std::vector<WorldLocalLight> finish(std::span<const WorldLocalLight> explicit_lights) {
    selected_.reserve(selected_.size()+explicit_lights.size());
    for(const auto& light:explicit_lights)selected_.push_back(candidate(light));
    std::sort(selected_.begin(),selected_.end(),before);
    std::vector<WorldLocalLight> lights;lights.reserve(selected_.size());
    for(const auto& value:selected_)lights.push_back(*value.light);
    return lights;
  }
};
}
