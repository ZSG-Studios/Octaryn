#include "DDGISystem.h"
#include "DDGIVolumeConfig.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>
using namespace octaryn::client::rendering;
static void require(bool condition,const char* message) {
  if(!condition) {std::fprintf(stderr,"ddgi_schedule_test=failed reason=%s\n",message);throw std::runtime_error(message);}
}
// Selection entries pack a ray tier in the top two bits; mask it for index checks.
static unsigned idx(unsigned packed) {return packed&0x3FFFFFFFu;}
int main() {
  DDGIConfig base;base.rays=176;
  const auto coarse128=ddgi_volume_config(base,128,false,8);
  require(coarse128.counts==std::array<unsigned,3>{32,12,32} && coarse128.spacing==8 && coarse128.budget==128,
      "coarse radius 128 did not restore its independent documented grid");
  for(unsigned radius:{64u,128u,192u,256u,512u,1024u}) {
    const auto config=ddgi_volume_config(base,radius,false,8);
    require(config.counts[0]<=64 && config.counts[1]==12 && config.counts[2]<=64 && config.budget<=256,
        "coarse radius created an unbounded voxel-resolution volume");
    require(config.counts[0]*config.spacing>=radius*2 && config.rays==176,
        "coarse radius silently truncated the requested grid span or changed ray quality");
  }
  const auto fine6=ddgi_volume_config(base,6,true,8);
  require(fine6.counts==std::array<unsigned,3>{12,12,12} && fine6.spacing==1 && fine6.budget==48,
      "coarse restoration changed fine voxel-cell density or its bounded budget");
  DDGISystem s;s.config.counts={4,4,4};s.config.spacing=4;s.config.budget=8;
  s.control_data.resize(64);s.last_updates.resize(64);s.dirty.resize(64,true);s.frame=1;
  ddgi_schedule(s,{-.1f,1,-.1f});
  require(s.selected.size()==8,"update budget was not respected");
  require(s.origin==std::array<int,3>{-3,-2,-3},"negative world coordinates did not floor correctly");
  std::set<std::array<int,3>> cells;
  for(const auto& control:s.control_data)cells.insert(control.cell);
  require(cells.size()==64,"toroidal grid aliased distinct logical probes");
  const auto old=s.control_data;
  std::fill(s.dirty.begin(),s.dirty.end(),false);++s.frame;
  ddgi_schedule(s,{4.1f,1,-.1f});
  unsigned preserved=0,changed=0;
  for(unsigned i=0;i<64;++i) {
    if(old[i].cell==s.control_data[i].cell) {
      ++preserved;require(old[i].version==s.control_data[i].version,"scroll discarded overlapping probe history");
    } else {
      ++changed;require(old[i].version!=s.control_data[i].version,"wrapped probe retained stale world history");
    }
  }
  require(preserved==32 && changed==32,"two-cell scroll exposed the wrong number of probes");
  std::fill(s.dirty.begin(),s.dirty.end(),false);s.config.max_distance=.1f;s.config.budget=1;
  const auto target=s.control_data[0].cell;
  const std::array<float,3> point{target[0]*4.f,target[1]*4.f,target[2]*4.f};
  const auto revision=s.control_data[0].version;
  ++s.frame;
  ddgi_invalidate(s,point,point);
  require(s.control_data[0].version==revision,"geometry refresh destroyed existing cell history");
  require(s.control_data[0].refresh_frame==s.frame,"geometry refresh failed to wake a sleeping probe");
  require(std::count(s.dirty.begin(),s.dirty.end(),true)==1,"small edit invalidated distant probes");
  ddgi_invalidate(s,point,point);
  require(s.control_data[0].version==revision,"pending invalidation churned probe generation");
  ++s.frame;ddgi_schedule(s,{4.1f,1,-.1f});
  require(idx(s.selected[0])==0,"edited geometry did not outrank near-camera stable probes");
  require(s.selected[0]>>30==0,"edited geometry lost its burst ray tier");
  s.config.budget=8;std::fill(s.dirty.begin(),s.dirty.end(),false);
  for(unsigned iteration=0;iteration<16;++iteration) {++s.frame;ddgi_schedule(s,{4.1f,1,-.1f});}
  require(std::count(s.last_updates.begin(),s.last_updates.end(),0)==0,
      "initial publication left probes uninitialized");
  std::fill(s.dirty.begin(),s.dirty.end(),false);
  ++s.frame;ddgi_schedule(s,{4.1f,1,-.1f});
  require(s.selected.empty(),"dormant probes still consumed the ray budget");
  // A converged probe sleeps through the long idle sweep; age past the maximum
  // jittered interval before expecting the mid-tier refresh to resume.
  s.last_update_times[0]=s.time_seconds-1.3*DDGIIdleInteriorSeconds;++s.frame;
  ddgi_schedule(s,{4.1f,1,-.1f});
  require(idx(s.selected[0])==0 && s.selected[0]>>30==1,
      "aged-out dormant probe did not resume at the mid ray tier");
  const auto stable=s.control_data;
  s.config.max_distance=64;
  for(unsigned iteration=0;iteration<40;++iteration) {
    ++s.frame;ddgi_invalidate(s,point,point);ddgi_schedule(s,{4.1f,1,-.1f});
    for(unsigned i=0;i<64;++i)
      require(s.control_data[i].version==stable[i].version,"streaming publications repeatedly discarded irradiance history");
  }
  require(sizeof(DDGIControl)==32 && sizeof(DDGIProbe)==32,"CPU probe layouts differ from shader storage");
  DDGISystem streaming;streaming.config.counts={16,8,16};streaming.config.budget=64;
  streaming.control_data.resize(2048);streaming.last_updates.resize(2048);streaming.dirty.resize(2048,true);
  streaming.frame=1;ddgi_schedule(streaming,{0,0,0});
  for(unsigned frame=0;frame<128;++frame) {
    ++streaming.frame;ddgi_invalidate(streaming,{-64,-64,-64},{64,64,64});ddgi_schedule(streaming,{0,0,0});
    std::set<unsigned> unique;for(auto packed:streaming.selected)unique.insert(idx(packed));
    require(unique.size()==64,"streaming refresh exceeded its budget or selected duplicate probes");
  }
  require(std::count(streaming.last_updates.begin(),streaming.last_updates.end(),0)==0,
      "continuous scene publications starved newly exposed probe cells");
  ++streaming.frame;ddgi_schedule(streaming,{3.9999f,0,0});
  const auto fade=streaming.fade_origin;
  ++streaming.frame;ddgi_schedule(streaming,{4.0001f,0,0});
  require(std::abs(streaming.fade_origin[0]-fade[0])<.0001f,"volume coverage snapped at a scrolling boundary");
  for(float camera=-8.01f;camera<8.1f;camera+=.13f) {
    ++streaming.frame;ddgi_schedule(streaming,{camera,camera,camera});
    for(unsigned axis=0;axis<3;++axis) {
      const float low=streaming.fade_origin[axis],high=low+float(streaming.config.counts[axis]-2);
      require(low>=float(streaming.origin[axis]) && high<=float(streaming.origin[axis]+streaming.config.counts[axis]-1),
          "continuous fade escaped available probe support");
    }
  }
  constexpr unsigned fineCount=1728;
  DDGISystem tunnel;tunnel.config.counts={12,12,12};tunnel.config.spacing=1;tunnel.config.budget=64;
  tunnel.config.max_distance=.1f;tunnel.cell_centered=true;tunnel.control_data.resize(fineCount);
  tunnel.last_updates.resize(fineCount);tunnel.dirty.resize(fineCount,true);tunnel.frame=1;
  ddgi_schedule(tunnel,{.5f,1.62f,.5f});
  unsigned tunnelProbe=fineCount;
  for(unsigned i=0;i<fineCount;++i)if(tunnel.control_data[i].cell==std::array<int,3>{0,0,0})tunnelProbe=i;
  require(tunnelProbe<fineCount,"fine cascade did not retain a voxel-center probe in a one-block tunnel");
  const float ceilingCoordinate=1.62f+1.5f-.5f;
  require(ceilingCoordinate-tunnel.fade_origin[1]>=2 && tunnel.fade_origin[1]+10-ceilingCoordinate>=2,
      "nearby tunnel ceiling falls outside full fine cascade coverage");
  std::fill(tunnel.dirty.begin(),tunnel.dirty.end(),false);
  ++tunnel.frame;ddgi_invalidate(tunnel,{.5f,.5f,.5f},{.5f,.5f,.5f});
  require(tunnel.dirty[tunnelProbe] && std::count(tunnel.dirty.begin(),tunnel.dirty.end(),true)==1,
      "dig invalidation missed the half-cell anchored probe");
  ++tunnel.frame;ddgi_schedule(tunnel,{.5f,1.62f,.5f});
  require(std::find_if(tunnel.selected.begin(),tunnel.selected.end(),
      [&](unsigned packed){return idx(packed)==tunnelProbe;})!=tunnel.selected.end(),
      "fine cascade did not immediately schedule the affected nearby tunnel probe");
  for(unsigned frame=0;frame<80;++frame) {++tunnel.frame;ddgi_schedule(tunnel,{.5f,1.62f,.5f});}
  const auto initialized=tunnel.control_data;
  std::fill(tunnel.dirty.begin(),tunnel.dirty.end(),false);tunnel.config.max_distance=64;
  ++tunnel.frame;ddgi_invalidate(tunnel,{0,0,0},{1,2,1});
  const auto editFrame=tunnel.frame;std::set<unsigned> refreshed;
  for(unsigned frame=0;frame<27;++frame) {
    ++tunnel.frame;ddgi_schedule(tunnel,{.5f,1.62f,.5f});
    for(auto packed:tunnel.selected)refreshed.insert(idx(packed));
  }
  require(refreshed.size()==fineCount,"fine tunnel edit failed to refresh its complete volume within 27 frames");
  for(unsigned i=0;i<fineCount;++i) {
    require(tunnel.control_data[i].version==initialized[i].version,"digging discarded retained fine irradiance history");
    require(tunnel.last_updates[i]>editFrame,"a fine probe retained a stale pre-dig schedule");
  }
  DDGISystem occupied;occupied.config.counts={4,4,4};occupied.config.spacing=1;occupied.config.budget=8;
  occupied.control_data.resize(64);occupied.last_updates.resize(64);occupied.dirty.resize(64,true);
  ddgi_scroll(occupied,{.5f,.5f,.5f});
  occupied.occupancy.assign(64,1);occupied.occupancy[3]=0;occupied.frame=1;
  ddgi_schedule(occupied,{.5f,.5f,.5f});
  require(occupied.selected.size()==1 && idx(occupied.selected[0])==3,"solid and sky cells consumed the update budget");
  occupied.ignore_active=true;occupied.ignore_voxel={0,0,0};occupied.occupancy[0]=0;
  occupied.control_data[0].padding[1]=1;occupied.dirty[0]=true;++occupied.frame;
  ddgi_schedule(occupied,{.5f,.5f,.5f});
  require(std::find_if(occupied.selected.begin(),occupied.selected.end(),
      [](unsigned packed){return idx(packed)==0;})==occupied.selected.end(),
      "speculative hole probe traced before neighbor irradiance was used");
  occupied.control_data[0].padding[1]=0;++occupied.frame;ddgi_schedule(occupied,{.5f,.5f,.5f});
  require(std::find_if(occupied.selected.begin(),occupied.selected.end(),
      [](unsigned packed){return idx(packed)==0;})!=occupied.selected.end(),
      "opened hole probe was not scheduled after the voxel actually emptied");
  DDGISystem sky;sky.config.counts={4,4,4};sky.config.spacing=8;sky.config.budget=8;
  sky.control_data.resize(64);sky.last_updates.resize(64);sky.dirty.resize(64,true);
  ddgi_scroll(sky,{.5f,.5f,.5f});
  sky.occupancy.assign(64,2);sky.frame=1;ddgi_schedule(sky,{.5f,.5f,.5f});
  require(sky.selected.size()==8,"open-sky probes were removed from the interpolation budget");
  for(unsigned i=0;i<8;++i) {++sky.frame;ddgi_schedule(sky,{.5f,.5f,.5f});}
  // Converged sky probes hold the long idle sweep; the environment is still
  // sampled after it, just no longer retraced twice a second forever.
  ++sky.frame;sky.time_seconds=float(1.3*DDGIIdleSkySeconds);ddgi_schedule(sky,{.5f,.5f,.5f});
  require(!sky.selected.empty(),"open-sky probes stopped tracking environment changes forever");
  for(unsigned fps:{30u,60u,144u}) {
    DDGISystem throughput;throughput.config.counts={4,4,4};throughput.config.budget=8;
    throughput.dispatch_capacity=64;throughput.control_data.resize(64);
    throughput.last_updates.resize(64);throughput.dirty.resize(64,true);unsigned work=0;
    for(unsigned frame=1;frame<=fps;++frame) {
      throughput.frame=frame;throughput.frame_seconds=1./fps;throughput.time_seconds=double(frame)/fps;
      std::fill(throughput.dirty.begin(),throughput.dirty.end(),true);
      ddgi_schedule(throughput,{0,0,0});work+=unsigned(throughput.selected.size());
      require(throughput.selected.size()<=64,"elapsed-time budget exceeded allocated dispatch capacity");
    }
    require(work>=479 && work<=480,"probe work per second changed with frame rate");
    DDGISystem timed;timed.config.counts={2,2,2};timed.config.spacing=1;timed.config.budget=8;
    timed.control_data.resize(8);timed.last_updates.resize(8);timed.dirty.resize(8,true);
    unsigned updates=0;double previous=0,maxGap=0;
    for(unsigned frame=1;frame<=fps*4;++frame) {
      timed.frame=frame;timed.time_seconds=double(frame)/fps;ddgi_schedule(timed,{0,0,0});
      if(timed.selected.empty())continue;
      if(updates++)maxGap=std::max(maxGap,timed.time_seconds-previous);
      previous=timed.time_seconds;
    }
    // Refresh stays time-based across frame rates: quick converge observations
    // after a disturbance, then a bounded idle sweep replaces the former fixed
    // 0.25 s cadence that retraced the whole field forever.
    require(updates>=8 && maxGap<=1.3*DDGIIdleInteriorSeconds+1./fps+1e-6,
        "probe refresh depended on FPS or retained 120-frame sleeps");
    ++timed.frame;
    ddgi_invalidate(timed,{-1,-1,-1},{1,1,1},0,true,true);
    ddgi_invalidate(timed,{-1,-1,-1},{1,1,1},0,true,false);
    ddgi_schedule(timed,{0,0,0});
    for(auto packed:timed.selected)
      require((timed.control_data[idx(packed)].padding[2]&DDGIHardReject)!=0,"hard removal was cleared before GPU recursive tracing");
    ++timed.frame;ddgi_schedule(timed,{0,0,0});
    require(std::all_of(timed.control_data.begin(),timed.control_data.end(),
        [](const DDGIControl& control){return control.padding[2]==0;}),"completed removal rejection never retired");
    const auto selectedFrame=timed.last_updates[0];timed.frame=selectedFrame;
    ddgi_invalidate(timed,{-1,-1,-1},{1,1,1},0,true,true);
    require(timed.control_data[0].refresh_frame>selectedFrame,"between-frame edit matched the old trace timestamp");
  }
  {
    // A converged static scene must settle into the long idle sweep instead of
    // retracing the whole field at the convergence cadence forever, which
    // saturated the trace budget and starved real light-change responses.
    DDGISystem steady;steady.config.counts={16,16,16};steady.config.budget=128;
    steady.config.rays=176;steady.dispatch_capacity=4096;
    const unsigned count=4096;
    steady.control_data.resize(count);steady.last_updates.resize(count);
    steady.last_update_times.resize(count);steady.dirty.resize(count,true);
    steady.occupancy.resize(count);
    for(unsigned frame=0;frame<60*8;++frame) {
      ++steady.frame;steady.frame_seconds=1./60;steady.time_seconds+=1./60;
      ddgi_schedule(steady,{8,8,8});
      const unsigned units=steady.stats.scheduled_rays+64*steady.stats.updated_probes;
      if(steady.stats.updated_probes)ddgi_budget_sample(steady,.045+units*1e-6,units,
        steady.stats.updated_probes,steady.selection_target);
    }
    require(std::count(steady.last_updates.begin(),steady.last_updates.end(),0)==0,
        "steady fixture never completed its initial sweep");
    unsigned long long work=0;
    for(unsigned frame=0;frame<60*4;++frame) {
      ++steady.frame;steady.frame_seconds=1./60;steady.time_seconds+=1./60;
      ddgi_schedule(steady,{8,8,8});
      const unsigned units=steady.stats.scheduled_rays+64*steady.stats.updated_probes;
      work+=steady.stats.updated_probes;
      if(steady.stats.updated_probes)ddgi_budget_sample(steady,.045+units*1e-6,units,
        steady.stats.updated_probes,steady.selection_target);
    }
    // Two idle sweeps land in a four-second window; the former convergence
    // cadence would have retraced the field sixteen times.
    require(work<unsigned(2.5*count),"static converged field kept saturating the trace budget");
    require(steady.stats.oldest_update_seconds>0.f,
        "steady fixture stopped tracking retained histories entirely");
  }
  std::puts("ddgi_schedule_test=passed cases=36 coarse_128_grid=1 coarse_radius_bounds=1 independent_fine_config=1 negative_coordinates=1 scroll_preservation=1 bounded_updates=1 edit_priority=1 timed_refresh_30_60_144=1 timed_budget_30_60_144=1 streaming_history=1 refresh_wakeup=1 streaming_initialization=1 continuous_coverage=1 tunnel_probe_anchor=1 tunnel_ceiling_coverage=1 tunnel_edit_refresh_frames=27 occupancy_skip=1 seed_before_trace=1 opened_trace=1 sky_refresh=1 removal_marker_lifetime=1 between_frame_edit=1 static_idle_sweep=1");
}
