#pragma once
#include "Probe.h"
#include "ResourceProbePacing.h"
namespace mesh_probe {
inline void drain_retirement(WorldRenderer& r) {
  const auto deadline=SDL_GetTicksNS()+2000000000ull;
  for(;;) {
    resource_probe::Frame frame("retirement_drain");
    open_world_renderer_retire_step(&r,32,1000.0);
    require(open_world_renderer_retirement_frame(&r),"retirement drain real GPU maintenance");
    if(open_world_renderer_retirement_remaining(&r)==0)break;
    require(SDL_GetTicksNS()<deadline,"asynchronous resource retirement did not drain in bounded time");
    SDL_Delay(1);
  }
  rhi::ResourceRetirementInfo info{};
  checked(r.queue->getResourceRetirementInfo(&info),"final resource retirement counters");
  require(info.pendingCount==0 && info.activeCount==0,"closing left queued or active resource deletion");
}
}
