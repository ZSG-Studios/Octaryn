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
void ddgi_invalidate(DDGISystem& s,const std::array<float,3>& minimum,const std::array<float,3>& maximum,float radius) {
  if(radius<0)radius=s.config.max_distance;
  // Record bounded regions for the dirty-region debug view; skip full-volume floods.
  if(minimum[0]>-1e29f) {
    auto& box=s.debug_boxes[s.debug_box_cursor%s.debug_boxes.size()];
    box.frame=s.frame;++s.debug_box_cursor;
    for(unsigned axis=0;axis<3;++axis) {box.minimum[axis]=minimum[axis]-radius;box.maximum[axis]=maximum[axis]+radius;}
  }
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
  unsigned needed=0;
  for(unsigned i=0;i<order.size();++i) {
    float distance2=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=(float(s.control_data[i].cell[axis])+(s.cell_centered?.5f:0.f))*s.config.spacing-camera[axis];distance2+=delta*delta;
    }
    const float age=float(s.frame-s.last_updates[i]);
    const float changed=s.dirty[i]?(s.last_updates[i]?200000.f:100000.f):0.f;
    const unsigned occupancy=i<s.occupancy.size()?s.occupancy[i]:0u;
    const bool seed_only=s.control_data[i].padding[1]!=0;
    const bool skip=occupancy==1 || seed_only;
    const float sky=occupancy==2?.2f:1.f;
    scores[i]=skip?-1e9f:sky*(changed+age*4.f+256.f/(1.f+distance2/(s.config.spacing*s.config.spacing)));
    if(!skip)++needed;
  }
  if(needed==0) {
    s.selected.clear();s.stats.updated_probes=0;s.stats.scheduled_rays=0;return;
  }
  const unsigned budget=std::min<unsigned>(s.config.budget,needed);
  const auto priority=[&](unsigned a,unsigned b){
    return scores[a]==scores[b]?a<b:scores[a]>scores[b];
  };
  std::partial_sort(order.begin(),order.begin()+budget,order.end(),priority);
  s.selected.assign(order.begin(),order.begin()+budget);
  // Streaming can refresh initialized probes every frame. Reserve part of the
  // same budget so new cells cannot remain black until streaming stops.
  std::vector<unsigned> fresh;
  for(unsigned i=0;i<order.size();++i)if(!s.last_updates[i] && scores[i]>-1e8f)fresh.push_back(i);
  const unsigned freshBudget=budget==1?(s.frame%4==1?1u:0u):std::min(budget,std::max(1u,budget/2));
  const unsigned reserve=std::min<unsigned>(unsigned(fresh.size()),freshBudget);
  std::partial_sort(fresh.begin(),fresh.begin()+reserve,fresh.end(),priority);
  unsigned initialized=0;
  for(unsigned i=0;i<reserve;++i) {
    if(std::find(s.selected.begin(),s.selected.end(),fresh[i])!=s.selected.end())continue;
    while(initialized<budget && !s.last_updates[s.selected[budget-1-initialized]])++initialized;
    if(initialized<budget)s.selected[budget-1-initialized++]=fresh[i];
  }
  // Pack a ray tier into each selection: fresh or dirty probes burst with the
  // full lighting ray count, recently updated probes keep 48 lighting rays,
  // and the quiet background converges slowly at 16. The GPU culls the rest.
  // Tiers read the pre-stamp state, so stamping happens inside the same pass.
  const unsigned fixedRays=std::min(64u,std::max(s.config.rays/4,s.config.rays-std::min(s.config.rays,48u)));
  const unsigned lighting=s.config.rays-fixedRays;
  unsigned scheduled=0;
  for(auto& entry:s.selected) {
    const unsigned index=entry;
    const unsigned tier=!s.last_updates[index]||s.dirty[index]?0u:(s.frame-s.last_updates[index]<480?1u:2u);
    s.last_updates[index]=s.frame;s.dirty[index]=false;
    entry=index|(tier<<30);
    scheduled+=fixedRays+(tier==0u?lighting:tier==1u?std::min(48u,lighting):std::min(16u,lighting));
  }
  s.stats.updated_probes=budget;s.stats.scheduled_rays=scheduled;
}
}
