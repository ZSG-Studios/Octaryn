#include "DDGISystem.h"
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
namespace {
constexpr double RefreshSeconds=.25,SkyRefreshSeconds=.5;
constexpr double ResponseSeconds=.1;
// Bounded observations that lean toward the new field before maturity resumes.
constexpr std::uint8_t ResponseObservations=4;
unsigned wrap(int value,unsigned count) {return unsigned((value%int(count)+int(count))%int(count));}
void invalidate(DDGISystem& s,unsigned index,bool lights,bool hard,bool refresh_history) {
  const auto refresh=static_cast<std::uint32_t>(std::max(s.frame,s.last_updates[index]+1));
  auto& control=s.control_data[index];
  if(lights) {
    // A new addition must not extend an already-traced removal's exclusion.
    if(refresh_history && !hard && (s.control_data[index].padding[2]&DDGIHardReject) &&
        s.last_updates[index] && s.last_updates[index]>=s.control_data[index].refresh_frame)
      control.padding[2]=(control.padding[2]&~DDGIHardReject)|DDGIGentleWake;
    const bool geometry_pending=control.refresh_frame>s.last_updates[index] &&
      !(control.padding[2]&DDGILightingOnly);
    control.padding[2]|=hard?DDGIHardReject:DDGIGentleWake;
    if(!geometry_pending)control.padding[2]|=DDGILightingOnly;
    if(hard || refresh_history)s.control_data[index].refresh_frame=refresh;
    if(hard || refresh_history)s.response_updates[index]=ResponseObservations;
    s.controls_dirty=true;
  }
  // Refresh bumps unconditionally: a probe dirtied elsewhere first (occupancy
  // flip, earlier wake) must still snap on retrace, or dug-out probes keep
  // solid-era daylight for minutes (white speckles in dark tunnels).
  if(!lights) {
    s.control_data[index].refresh_frame=refresh;
    control.padding[2]&=~DDGILightingOnly;
    s.response_updates[index]=ResponseObservations;
    s.controls_dirty=true;
  }
  if(!s.dirty[index]) {
    s.dirty[index]=true;s.controls_dirty=true;++s.stats.invalidated_probes;
  }
}
}
void ddgi_environment_changed(DDGISystem& s,bool abrupt) {
  // Gradual drift is already sampled by the ordinary age-based cadence. Forcing
  // a global fast interval there would re-inject ray noise every frame without
  // tracking the sun any better, so only an abrupt change requests a response.
  if(!abrupt)return;
  const auto refresh=static_cast<std::uint32_t>(std::max(s.frame,1ull));
  s.response_updates.resize(s.control_data.size());
  for(unsigned i=0;i<s.control_data.size();++i) {
    // Only sky-visible probes re-blend quickly: their radiance tracks the sun
    // directly. Interior probes keep the mature per-observation filter, so a
    // moving sun cannot flicker solid faces with half-blended noisy samples.
    if(s.control_data[i].padding[1]||(i<s.occupancy.size()&&s.occupancy[i]!=2))continue;
    auto& control=s.control_data[i];
    const bool geometry_pending=control.refresh_frame>s.last_updates[i]&&
      !(control.padding[2]&DDGILightingOnly);
    control.padding[2]|=DDGIGentleWake;
    if(!geometry_pending)control.padding[2]|=DDGILightingOnly;
    control.refresh_frame=std::max(control.refresh_frame,refresh);
    if(i<s.response_updates.size())s.response_updates[i]=ResponseObservations;
  }
  // No global dirty burst: responding probes stay eligible at the response
  // cadence and are prioritized by age, so the sweep is a smooth budget-bounded
  // wave rather than a stampede that re-echoes across the whole field.
  s.controls_dirty=true;
}
void ddgi_budget_sample(DDGISystem& s,double milliseconds,unsigned work,unsigned probes,unsigned target) {
  if(!work || !std::isfinite(milliseconds) || milliseconds<=0)return;
  if(probes && probes<target) {
    // A short eligible tail pays fixed dispatch overhead, not the marginal cost
    // of a full batch. Charge its excess GPU time without poisoning throughput.
    const double cost=s.milliseconds_per_work>0?s.milliseconds_per_work:
      .35/(std::max(1u,s.config.budget)*double(s.config.rays+64));
    const double probe_cost=cost*(s.config.rays+64);
    const double excess=std::max(0.,milliseconds-probe_cost*probes);
    const double banked=std::min(s.budget_credit,excess/probe_cost);
    s.budget_credit-=banked;
    s.gpu_debt_seconds+=(excess-banked*probe_cost)/(s.config.gpu_budget_milliseconds*60);
    return;
  }
  const double sample=milliseconds/work;
  // React immediately to expensive work; release headroom slowly after it falls.
  s.milliseconds_per_work=s.milliseconds_per_work>0?
    std::max(sample,s.milliseconds_per_work*.9+sample*.1):sample;
  const double affordable=s.config.gpu_budget_milliseconds/(s.milliseconds_per_work*(s.config.rays+64));
  const double previous=s.adaptive_budget>0?s.adaptive_budget:s.config.budget*(s.config.gpu_budget_milliseconds/.35);
  s.adaptive_budget=std::max(1./60,std::min(affordable,previous*1.125));
}
void ddgi_invalidate(DDGISystem& s,const std::array<float,3>& minimum,const std::array<float,3>& maximum,float radius,bool lights,bool hard,bool refresh_history) {
  s.response_updates.resize(s.control_data.size());
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
    if(distance2<=radius*radius)invalidate(s,i,lights,hard,refresh_history);
  }
}
void ddgi_scroll(DDGISystem& s,const std::array<float,3>& camera) {
  const auto counts=s.config.counts;
  const auto previous_origin=s.origin;
  for(unsigned axis=0;axis<3;++axis) {
    const float coordinate=camera[axis]/s.config.spacing-(s.cell_centered?.5f:0.f);
    const int centerShift=s.cell_centered?1:0;
    s.origin[axis]=int(std::floor(coordinate))-int(counts[axis]/2)+centerShift;
    s.fade_origin[axis]=counts[axis]>2?coordinate-float(counts[axis]/2)+float(centerShift):float(s.origin[axis]);
  }
  // Fade follows subcell motion; only an exposed plane needs a slot/version walk.
  if(s.initialized && s.origin==previous_origin)return;
  for(unsigned z=0;z<counts[2];++z)for(unsigned y=0;y<counts[1];++y)for(unsigned x=0;x<counts[0];++x) {
    const std::array<int,3> cell{s.origin[0]+int(x),s.origin[1]+int(y),s.origin[2]+int(z)};
    const unsigned index=wrap(cell[0],counts[0])+counts[0]*(wrap(cell[1],counts[1])+counts[1]*wrap(cell[2],counts[2]));
    if(!s.initialized || s.control_data[index].cell!=cell) {
      s.control_data[index].cell=cell;
      ++s.control_data[index].version;s.dirty[index]=true;s.last_updates[index]=0;s.controls_dirty=true;
      s.control_data[index].refresh_frame=0;s.control_data[index].padding[2]=0;
      if(index<s.response_updates.size())s.response_updates[index]=0;
      if(index<s.last_update_times.size())s.last_update_times[index]=s.initialized?s.time_seconds:0;
      // Slot now backs a different logical probe; stale occupancy must not read
      // as a solidity flip. 3 = unknown, adopted silently by classification.
      if(index<s.occupancy.size())s.occupancy[index]=3;
    }
  }
  s.initialized=true;
}
void ddgi_schedule(DDGISystem& s,const std::array<float,3>& camera) {
  s.last_update_times.resize(s.control_data.size());
  s.response_updates.resize(s.control_data.size());
  ddgi_scroll(s,camera);
  s.stats.pending_probes=0;s.stats.oldest_update_seconds=0;
  const bool bursting=s.burst_frames!=0;
  if(s.burst_frames)--s.burst_frames;
  double demand=0;
  std::vector<float> scores(s.control_data.size());
  std::vector<unsigned> order;
  order.reserve(s.control_data.size());
  for(unsigned i=0;i<s.control_data.size();++i) {
    const unsigned occupancy=i<s.occupancy.size()?s.occupancy[i]:0u;
    const bool seed_only=s.control_data[i].padding[1]!=0;
    // Keep hard-removal rejection through the preceding trace/update dispatch.
    if(s.control_data[i].padding[2] && s.last_updates[i] && s.last_updates[i]<s.frame &&
        s.last_updates[i]>=s.control_data[i].refresh_frame &&
        (!s.dirty[i] || (s.control_data[i].padding[2]&DDGIHardReject))) {
      s.control_data[i].padding[2]=s.dirty[i]?(DDGIGentleWake|(s.control_data[i].padding[2]&DDGILightingOnly)):0u;
      s.controls_dirty=true;
    }
    const double age=s.time_seconds-s.last_update_times[i];
    const bool responding=s.response_updates[i]!=0;
    const double interval=responding?ResponseSeconds:
      occupancy==2?SkyRefreshSeconds:RefreshSeconds;
    if(occupancy!=1 && !seed_only) {
      const bool urgent=s.dirty[i] && (bursting || s.control_data[i].padding[2]);
      demand+=1./(60*(urgent?ResponseSeconds:interval));
      if(s.dirty[i] || !s.last_updates[i])++s.stats.pending_probes;
      if(s.last_updates[i])s.stats.oldest_update_seconds=std::max(s.stats.oldest_update_seconds,float(age));
    }
    const bool sleeping=s.last_updates[i] && !s.dirty[i] &&
      age<interval;
    if(occupancy==1 || seed_only || sleeping) {
      scores[i]=-1e9f;continue;
    }
    float distance2=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=(float(s.control_data[i].cell[axis])+(s.cell_centered?.5f:0.f))*s.config.spacing-camera[axis];
      distance2+=delta*delta;
    }
    const float changed=s.dirty[i]?(s.last_updates[i]?200000.f:100000.f):0.f;
    scores[i]=changed+float(age)*1024.f+256.f/(1.f+distance2/(s.config.spacing*s.config.spacing));
    order.push_back(i);
  }
  const double elapsed=std::clamp(s.frame_seconds,0.,.1);
  const double repayment=std::min(elapsed,s.gpu_debt_seconds);
  s.gpu_debt_seconds-=repayment;
  if(order.empty()) {
    s.selected.clear();s.budget_credit=0;s.stats.updated_probes=0;s.stats.scheduled_rays=0;return;
  }
  const unsigned cap=s.dispatch_capacity?s.dispatch_capacity:s.config.budget;
  // The configured budget is work per 1/60 second; capacity remains the hard
  // per-dispatch limit. Do not bank idle time into a later unbounded burst.
  const double initial_budget=s.config.budget*(s.config.gpu_budget_milliseconds/.35);
  const double rate=s.adaptive_budget>0?std::min(s.adaptive_budget,std::max(demand,initial_budget)):initial_budget;
  s.budget_credit=std::min(double(cap),s.budget_credit+rate*(elapsed-repayment)*60);
  // Timestamp/dispatch overhead is not per-probe work. At uncapped frame rates,
  // shrinking a one-probe dispatch makes its measured cost/probe grow forever.
  // Spend accumulated credit in amortized batches, never borrowing future work.
  const unsigned amortized=unsigned(std::ceil(std::min(double(cap),std::max(64.,rate))));
  const bool measured=s.adaptive_budget>0 || s.timing.milliseconds_per_tick>0;
  const unsigned batch=measured?std::min(amortized,unsigned(order.size())):1u;
  if(s.budget_credit+1e-9<batch) {
    s.selected.clear();s.stats.updated_probes=s.stats.scheduled_rays=0;return;
  }
  const unsigned budget=std::min({cap,unsigned(s.budget_credit+1e-9),unsigned(order.size())});
  s.selection_target=measured?amortized:budget;
  s.budget_credit=std::max(0.,s.budget_credit-budget);
  if(!budget) {s.selected.clear();s.stats.updated_probes=s.stats.scheduled_rays=0;return;}
  const auto oldest=[&](unsigned a,unsigned b){
    if(s.last_update_times[a]!=s.last_update_times[b])return s.last_update_times[a]<s.last_update_times[b];
    if(s.last_updates[a]!=s.last_updates[b])return s.last_updates[a]<s.last_updates[b];
    return scores[a]==scores[b]?a<b:scores[a]>scores[b];
  };
  const auto priority=[&](unsigned a,unsigned b){
    const unsigned laneA=s.dirty[a]?(s.last_updates[a]?2u:1u):s.response_updates[a]?2u:0u;
    const unsigned laneB=s.dirty[b]?(s.last_updates[b]?2u:1u):s.response_updates[b]?2u:0u;
    return laneA==laneB?oldest(a,b):laneA>laneB;
  };
  auto aged=order;
  std::partial_sort(aged.begin(),aged.begin()+budget,aged.end(),oldest);
  std::vector<unsigned> fresh;
  // Fresh sky is already neighbor-seeded (plausible environment light), so it
  // must not displace uninitialized geometry probes from the burst reserve.
  for(unsigned i:order)if(!s.last_updates[i] && (i>=s.occupancy.size()||s.occupancy[i]!=2))fresh.push_back(i);
  const unsigned reserve=std::min<unsigned>(unsigned(fresh.size()),budget);
  std::partial_sort(fresh.begin(),fresh.begin()+reserve,fresh.end(),priority);
  std::partial_sort(order.begin(),order.begin()+budget,order.end(),priority);
  s.selected.clear();std::vector<bool> chosen(s.control_data.size());
  unsigned nextPriority=0,nextFresh=0,nextAged=0;
  const auto take=[&](const std::vector<unsigned>& list,unsigned& next,unsigned end){
    while(next<end && chosen[list[next]])++next;
    if(next==end)return false;
    const unsigned index=list[next++];chosen[index]=true;s.selected.push_back(index);return true;
  };
  for(unsigned slot=0;slot<budget;++slot) {
    // Service slots, not rendered frames: fractional budgets cannot alias a lane.
    const unsigned lane=unsigned(s.scheduled_work++%4);
    if(lane==3 && take(aged,nextAged,budget))continue;
    if(lane==1 && take(fresh,nextFresh,reserve))continue;
    take(order,nextPriority,budget);
  }
  const unsigned fixedRays=std::min(64u,std::max(s.config.rays/4,s.config.rays-std::min(s.config.rays,48u)));
  const unsigned lighting=s.config.rays-fixedRays;
  unsigned scheduled=0;
  for(auto& entry:s.selected) {
    const unsigned index=entry;
    // New cells and explicit resets use the full ray set, including open sky.
    // Ordinary sky refresh uses background rays; indoor refresh uses active rays.
    const unsigned occupancy_now=index<s.occupancy.size()?s.occupancy[index]:0u;
    // New cells and explicit geometry resets use the full ray set, including
    // open sky. Ordinary sky refresh uses background rays; indoor refresh uses
    // active rays.
    const bool geometry_change=s.control_data[index].refresh_frame>s.last_updates[index] &&
      !(s.control_data[index].padding[2]&DDGILightingOnly);
    const bool snap_now=!s.last_updates[index]||geometry_change;
    unsigned tier;
    if(snap_now)tier=0u;
    else if(occupancy_now==2)tier=2u;
    else tier=1u;
    s.last_update_times[index]=s.time_seconds;
    s.last_updates[index]=s.frame;s.dirty[index]=false;
    if(s.response_updates[index])--s.response_updates[index];
    entry=index|(tier<<30);
    scheduled+=fixedRays+(tier==0u?lighting:tier==1u?std::min(48u,lighting):std::min(16u,lighting));
  }
  for(unsigned i=0;i<s.control_data.size();++i)if(s.dirty[i] && s.control_data[i].padding[2] &&
      (i>=s.occupancy.size()||s.occupancy[i]!=1) && !s.control_data[i].padding[1]) {
    s.burst_frames=std::max(s.burst_frames,1u);break;
  }
  s.stats.updated_probes=budget;s.stats.scheduled_rays=scheduled;
}
}
