#include "DDGISystem.h"
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
int main() {
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
  require(s.selected[0]==0,"edited geometry did not outrank near-camera stable probes");
  std::set<unsigned> visited;
  for(unsigned iteration=0;iteration<100;++iteration) {
    ++s.frame;ddgi_schedule(s,{4.1f,1,-.1f});visited.insert(s.selected[0]);
  }
  require(visited.size()==64,"age scheduling starved distant stable probes");
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
    const std::set<unsigned> unique(streaming.selected.begin(),streaming.selected.end());
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
  require(std::find(tunnel.selected.begin(),tunnel.selected.end(),tunnelProbe)!=tunnel.selected.end(),
      "fine cascade did not immediately schedule the affected nearby tunnel probe");
  for(unsigned frame=0;frame<80;++frame) {++tunnel.frame;ddgi_schedule(tunnel,{.5f,1.62f,.5f});}
  const auto initialized=tunnel.control_data;
  std::fill(tunnel.dirty.begin(),tunnel.dirty.end(),false);tunnel.config.max_distance=64;
  ++tunnel.frame;ddgi_invalidate(tunnel,{0,0,0},{1,2,1});
  const auto editFrame=tunnel.frame;std::set<unsigned> refreshed;
  for(unsigned frame=0;frame<27;++frame) {
    ++tunnel.frame;ddgi_schedule(tunnel,{.5f,1.62f,.5f});
    refreshed.insert(tunnel.selected.begin(),tunnel.selected.end());
  }
  require(refreshed.size()==fineCount,"fine tunnel edit failed to refresh its complete volume within 27 frames");
  for(unsigned i=0;i<fineCount;++i) {
    require(tunnel.control_data[i].version==initialized[i].version,"digging discarded retained fine irradiance history");
    require(tunnel.last_updates[i]>editFrame,"a fine probe retained a stale pre-dig schedule");
  }
  std::puts("ddgi_schedule_test=passed cases=21 negative_coordinates=1 scroll_preservation=1 bounded_updates=1 edit_priority=1 age_fairness=1 streaming_history=1 refresh_wakeup=1 streaming_initialization=1 continuous_coverage=1 tunnel_probe_anchor=1 tunnel_ceiling_coverage=1 tunnel_edit_refresh_frames=27");
}
