#pragma once
#include "WorldRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>

namespace octaryn::client::app {
struct StreamFramePacing {
  std::uint64_t work_ns{},sleep_ns{};
  bool success{true};
};

inline StreamFramePacing pace_world_stream(rendering::WorldRenderer* renderer,
    world_presentation::WorldStream& stream,std::uint64_t delay,bool pending) {
  StreamFramePacing result;
  const auto deadline=SDL_GetTicksNS()+delay;
  constexpr std::uint64_t reserve=2000000,work_limit=10000000,slice_limit=2000000;
  // Use spare cap time for private GPU stages. Publication remains before
  // camera queries and at renderer frame head; this never advances gameplay.
  for(unsigned slice=0;pending && slice<16 && result.work_ns<work_limit;++slice) {
    const auto now=SDL_GetTicksNS();
    if(now>=deadline || deadline-now<=reserve)break;
    const auto budget=std::min({slice_limit,deadline-now-reserve,work_limit-result.work_ns});
    if(!rendering::open_world_renderer_stream_progress(renderer,stream,double(budget)/1e6)) {
      result.success=false;return result;
    }
    result.work_ns+=SDL_GetTicksNS()-now;
    const auto sleep_start=SDL_GetTicksNS();
    if(sleep_start>=deadline)break;
    SDL_DelayNS(std::min<std::uint64_t>(1000000,deadline-sleep_start));
    result.sleep_ns+=SDL_GetTicksNS()-sleep_start;
  }
  const auto now=SDL_GetTicksNS();
  if(now<deadline) {
    SDL_DelayNS(deadline-now);
    result.sleep_ns+=SDL_GetTicksNS()-now;
  }
  return result;
}
}
