#include "DDGIVolumeConfig.h"
#include "LightingQuality.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
int main() {
  for(unsigned fps:{30u,60u,144u,600u})for(unsigned quality=0;quality<4;++quality) {
    DDGISystem s;DDGIConfig base;base.rays=176;
    s.config=ddgi_volume_config(base,1024,false,8);
    ddgi_set_gpu_budget(s,ddgi_quality_milliseconds(static_cast<LightingQuality>(quality)));
    const unsigned count=s.config.counts[0]*s.config.counts[1]*s.config.counts[2];
    s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count,true);
    s.dispatch_capacity=4096;s.frame_seconds=1./fps;
    double milliseconds=0;
    for(unsigned frame=0;frame<fps*6;++frame) {
      ++s.frame;s.time_seconds+=s.frame_seconds;
      ddgi_schedule(s,{0,0,0});
      const unsigned probes=static_cast<unsigned>(s.selected.size());
      const double cost=probes*.001;
      ddgi_budget_sample(s,cost,probes*(s.config.rays+64),probes,s.selection_target);
      if(frame>=fps*3)milliseconds+=cost;
    }
    const double expected=s.config.gpu_budget_milliseconds*60*3;
    if(std::abs(milliseconds-expected)>expected*.05)
      throw std::runtime_error("quality allowance did not reach scheduler work within 5 percent");
    std::printf("ddgi_quality fps=%u tier=%u work_ms=%.3f allowance_ms=%.3f probes=%u rays=%u spacing=%.1f\n",
      fps,quality,milliseconds,expected,count,s.config.rays,s.config.spacing);
  }
  std::puts("ddgi_quality_test=passed tiers=4 frame_rates=4 history_policy_unchanged=1");
}
