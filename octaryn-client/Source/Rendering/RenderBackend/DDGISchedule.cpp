#include "DDGISystem.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace octaryn::client::rendering {
namespace {
constexpr std::uint64_t SleepFrames=120;
unsigned wrap(int value,unsigned count) {return unsigned((value%int(count)+int(count))%int(count));}
void invalidate(DDGISystem& s,unsigned index,bool lights,bool hard) {
  if(lights) {
    s.control_data[index].padding[2]=hard?2u:1u;
    if(hard)s.control_data[index].refresh_frame=static_cast<std::uint32_t>(s.frame);
    s.controls_dirty=true;
  }
  // Refresh bumps unconditionally: a probe dirtied elsewhere first (occupancy
  // flip, earlier wake) must still snap on retrace, or dug-out probes keep
  // solid-era daylight for minutes (white speckles in dark tunnels).
  if(!lights)s.control_data[index].refresh_frame=static_cast<std::uint32_t>(s.frame);
  if(!s.dirty[index]) {
    s.dirty[index]=true;s.controls_dirty=true;++s.stats.invalidated_probes;
  }
}
}
void ddgi_invalidate(DDGISystem& s,const std::array<float,3>& minimum,const std::array<float,3>& maximum,float radius,bool lights,bool hard) {
  if(radius<0)radius=s.config.max_distance;
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
    if(distance2<=radius*radius)invalidate(s,i,lights,hard);
  }
}
void ddgi_scroll(DDGISystem& s,const std::array<float,3>& camera) {
  const auto counts=s.config.counts;
  for(unsigned axis=0;axis<3;++axis) {
    const float coordinate=camera[axis]/s.config.spacing-(s.cell_centered?.5f:0.f);
    const int centerShift=s.cell_centered?1:0;
    s.origin[axis]=int(std::floor(coordinate))-int(counts[axis]/2)+centerShift;
    s.fade_origin[axis]=counts[axis]>2?coordinate-float(counts[axis]/2)+float(centerShift):float(s.origin[axis]);
  }
  for(unsigned z=0;z<counts[2];++z)for(unsigned y=0;y<counts[1];++y)for(unsigned x=0;x<counts[0];++x) {
    const std::array<int,3> cell{s.origin[0]+int(x),s.origin[1]+int(y),s.origin[2]+int(z)};
    const unsigned index=wrap(cell[0],counts[0])+counts[0]*(wrap(cell[1],counts[1])+counts[1]*wrap(cell[2],counts[2]));
    if(!s.initialized || s.control_data[index].cell!=cell) {
      s.control_data[index].cell=cell;
      ++s.control_data[index].version;s.dirty[index]=true;s.last_updates[index]=0;s.controls_dirty=true;
      // Slot now backs a different logical probe; stale occupancy must not read
      // as a solidity flip. 3 = unknown, adopted silently by classification.
      if(index<s.occupancy.size())s.occupancy[index]=3;
    }
  }
  s.initialized=true;
}
void ddgi_schedule(DDGISystem& s,const std::array<float,3>& camera) {
  ddgi_scroll(s,camera);
  const bool bursting=s.burst_frames!=0;
  std::vector<float> scores(s.control_data.size());
  std::vector<unsigned> order;
  order.reserve(s.control_data.size());
  for(unsigned i=0;i<s.control_data.size();++i) {
    const unsigned occupancy=i<s.occupancy.size()?s.occupancy[i]:0u;
    const bool seed_only=s.control_data[i].padding[1]!=0;
    const bool sleeping=s.last_updates[i] && !s.dirty[i] && s.frame-s.last_updates[i]<SleepFrames;
    // Clean open-air probes never trace: seeded environment values are already
    // right, and any light/geometry change dirties them explicitly. Fresh ones
    // stay out of the burst reserve too, so geometry owns the budget.
    const bool openSky=occupancy==2 && !s.dirty[i];
    if(occupancy==1 || seed_only || sleeping || openSky || (bursting && !s.dirty[i] && s.last_updates[i])) {
      scores[i]=-1e9f;continue;
    }
    float distance2=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=(float(s.control_data[i].cell[axis])+(s.cell_centered?.5f:0.f))*s.config.spacing-camera[axis];
      distance2+=delta*delta;
    }
    const float age=float(s.frame-s.last_updates[i]);
    const float changed=s.dirty[i]?(s.last_updates[i]?200000.f:100000.f):0.f;
    const float sky=occupancy==2?.2f:1.f;
    scores[i]=sky*(changed+age*4.f+256.f/(1.f+distance2/(s.config.spacing*s.config.spacing)));
    order.push_back(i);
  }
  if(order.empty()) {
    s.selected.clear();s.stats.updated_probes=0;s.stats.scheduled_rays=0;return;
  }
  const unsigned cap=s.dispatch_capacity?s.dispatch_capacity:s.config.budget;
  const unsigned budget=std::min(s.burst_frames?cap:s.config.budget,unsigned(order.size()));
  if(s.burst_frames)--s.burst_frames;
  const auto priority=[&](unsigned a,unsigned b){
    return scores[a]==scores[b]?a<b:scores[a]>scores[b];
  };
  std::partial_sort(order.begin(),order.begin()+budget,order.end(),priority);
  s.selected.assign(order.begin(),order.begin()+budget);
  std::vector<unsigned> fresh;
  // Fresh sky is already neighbor-seeded (plausible environment light), so it
  // must not displace uninitialized geometry probes from the burst reserve.
  for(unsigned i:order)if(!s.last_updates[i] && (i>=s.occupancy.size()||s.occupancy[i]!=2))fresh.push_back(i);
  const unsigned freshBudget=budget==1?(s.frame%4==1?1u:0u):std::min(budget,std::max(1u,budget/2));
  const unsigned reserve=std::min<unsigned>(unsigned(fresh.size()),freshBudget);
  std::partial_sort(fresh.begin(),fresh.begin()+reserve,fresh.end(),priority);
  unsigned initialized=0;
  for(unsigned i=0;i<reserve;++i) {
    if(std::find(s.selected.begin(),s.selected.end(),fresh[i])!=s.selected.end())continue;
    while(initialized<budget && !s.last_updates[s.selected[budget-1-initialized]])++initialized;
    if(initialized<budget)s.selected[budget-1-initialized++]=fresh[i];
  }
  const unsigned fixedRays=std::min(64u,std::max(s.config.rays/4,s.config.rays-std::min(s.config.rays,48u)));
  const unsigned lighting=s.config.rays-fixedRays;
  unsigned scheduled=0;
  for(auto& entry:s.selected) {
    const unsigned index=entry;
    // Open-air probes hold environment light: neighbor seeding already makes
    // them plausible, so sky validates at background cost and never spends the
    // burst. Snapped work (fresh, geometry edits, hard light removals) bursts;
    // other dirty work (gentle light wakes) blends at active tier.
    const unsigned occupancy_now=index<s.occupancy.size()?s.occupancy[index]:0u;
    const bool snap_now=!s.last_updates[index]||s.control_data[index].refresh_frame>s.last_updates[index];
    unsigned tier;
    if(occupancy_now==2)tier=2u;
    else if(snap_now)tier=0u;
    else if(s.dirty[index])tier=1u;
    else tier=(s.frame-s.last_updates[index]<480)?1u:2u;
    s.last_updates[index]=s.frame;s.dirty[index]=false;
    if(s.control_data[index].padding[2]) {
      s.control_data[index].padding[2]=0;s.controls_dirty=true;
    }
    entry=index|(tier<<30);
    scheduled+=fixedRays+(tier==0u?lighting:tier==1u?std::min(48u,lighting):std::min(16u,lighting));
  }
  for(unsigned i=0;i<s.control_data.size();++i)if(s.control_data[i].padding[2]) {
    s.burst_frames=std::max(s.burst_frames,1u);break;
  }
  s.stats.updated_probes=budget;s.stats.scheduled_rays=scheduled;
}
}
