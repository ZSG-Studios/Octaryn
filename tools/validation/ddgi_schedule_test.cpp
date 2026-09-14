#include "DDGISystem.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <vector>
using namespace octaryn::client::rendering;
static void require(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
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
  std::puts("ddgi_schedule_test=passed cases=11 negative_coordinates=1 scroll_preservation=1 bounded_updates=1 edit_priority=1 age_fairness=1 streaming_history=1 refresh_wakeup=1");
}
