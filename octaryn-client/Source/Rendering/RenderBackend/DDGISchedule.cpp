#include "DDGISystem.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace octaryn::client::rendering {
namespace {
unsigned wrap(int value,unsigned count) {return unsigned((value%int(count)+int(count))%int(count));}
void invalidate(DDGISystem& s,unsigned index) {
  if(!s.dirty[index]) {
    // A scene refresh keeps this cell's irradiance and relocation alive while
    // the bounded scheduler retraces it. Only scrolling changes cell identity.
    s.control_data[index].refresh_frame=static_cast<std::uint32_t>(s.frame);
    s.dirty[index]=true;s.controls_dirty=true;++s.stats.invalidated_probes;
  }
}
}
void ddgi_invalidate(DDGISystem& s,const std::array<float,3>& minimum,const std::array<float,3>& maximum) {
  // Visibility changes can affect every probe whose rays could reach the edit.
  const float radius=s.config.max_distance;
  for(unsigned i=0;i<s.control_data.size();++i) {
    float distance2=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float position=(float(s.control_data[i].cell[axis])+(s.cell_centered?.5f:0.f))*s.config.spacing;
      const float delta=std::max({minimum[axis]-position,position-maximum[axis],0.f});
      distance2+=delta*delta;
    }
    if(distance2<=radius*radius)invalidate(s,i);
  }
}
void ddgi_schedule(DDGISystem& s,const std::array<float,3>& camera) {
  const auto counts=s.config.counts;
  for(unsigned axis=0;axis<3;++axis) {
    const float coordinate=camera[axis]/s.config.spacing-(s.cell_centered?.5f:0.f);
    const int centerShift=s.cell_centered?1:0;
    s.origin[axis]=int(std::floor(coordinate))-int(counts[axis]/2)+centerShift;
    // Keep a guard cell for continuous fading across discrete grid scrolls.
    s.fade_origin[axis]=counts[axis]>2?coordinate-float(counts[axis]/2)+float(centerShift):float(s.origin[axis]);
  }
  for(unsigned z=0;z<counts[2];++z)for(unsigned y=0;y<counts[1];++y)for(unsigned x=0;x<counts[0];++x) {
    const std::array<int,3> cell{s.origin[0]+int(x),s.origin[1]+int(y),s.origin[2]+int(z)};
    const unsigned index=wrap(cell[0],counts[0])+counts[0]*(wrap(cell[1],counts[1])+counts[1]*wrap(cell[2],counts[2]));
    if(!s.initialized || s.control_data[index].cell!=cell) {
      // A wrapped physical slot belongs to a different logical world probe.
      s.control_data[index].cell=cell;
      ++s.control_data[index].version;s.dirty[index]=true;s.last_updates[index]=0;s.controls_dirty=true;
    }
  }
  s.initialized=true;
  std::vector<float> scores(s.control_data.size());
  std::vector<unsigned> order(s.control_data.size());std::iota(order.begin(),order.end(),0u);
  for(unsigned i=0;i<order.size();++i) {
    float distance2=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=(float(s.control_data[i].cell[axis])+(s.cell_centered?.5f:0.f))*s.config.spacing-camera[axis];distance2+=delta*delta;
    }
    const float age=float(s.frame-s.last_updates[i]);
    const float changed=s.dirty[i]?(s.last_updates[i]?200000.f:100000.f):0.f;
    scores[i]=changed+age*4.f+256.f/(1.f+distance2/(s.config.spacing*s.config.spacing));
  }
  const unsigned budget=std::min<unsigned>(s.config.budget,unsigned(order.size()));
  const auto priority=[&](unsigned a,unsigned b){
    return scores[a]==scores[b]?a<b:scores[a]>scores[b];
  };
  std::partial_sort(order.begin(),order.begin()+budget,order.end(),priority);
  s.selected.assign(order.begin(),order.begin()+budget);
  // Streaming can refresh initialized probes every frame. Reserve part of the
  // same budget so new cells cannot remain black until streaming stops.
  std::vector<unsigned> fresh;
  for(unsigned i=0;i<order.size();++i)if(!s.last_updates[i])fresh.push_back(i);
  const unsigned freshBudget=budget==1?(s.frame%4==1?1u:0u):std::min(budget,std::max(1u,budget/4));
  const unsigned reserve=std::min<unsigned>(unsigned(fresh.size()),freshBudget);
  std::partial_sort(fresh.begin(),fresh.begin()+reserve,fresh.end(),priority);
  unsigned initialized=0;
  for(unsigned i=0;i<reserve;++i) {
    if(std::find(s.selected.begin(),s.selected.end(),fresh[i])!=s.selected.end())continue;
    while(initialized<budget && !s.last_updates[s.selected[budget-1-initialized]])++initialized;
    if(initialized<budget)s.selected[budget-1-initialized++]=fresh[i];
  }
  s.stats.updated_probes=budget;s.stats.scheduled_rays=budget*s.config.rays;
  for(auto index:s.selected) {s.last_updates[index]=s.frame;s.dirty[index]=false;}
}
}
