#pragma once
#include "Probe.h"
#include <chrono>
#include <cstring>

namespace mesh_probe {
class BlockTransportRasterTrace {
  const WorldRenderer& renderer;
  const char* scope;
  bool enabled=false;
  const std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
public:
  BlockTransportRasterTrace(const WorldRenderer& r,const char* name):renderer(r),scope(name) {
    const auto* option=SDL_getenv("OCTARYN_CLIENT_BT_RASTER_DIAGNOSTICS");
    enabled=r.gi_mode==GiMode::BlockTransport && option && std::strcmp(option,"1")==0;
    mark("begin");
  }
  ~BlockTransportRasterTrace() {mark("locals_destroyed");}
  void mark(const char* phase,unsigned detail=0)const {
    if(!enabled)return;
    std::printf("block_transport_raster_phase scope=%s phase=%s detail=%u frame=%llu slot=%u depth0=%u depth1=%u elapsed_ms=%.3f\n",
        scope,phase,detail,static_cast<unsigned long long>(renderer.frames),renderer.active_frame,
        renderer.targets[0].depth && renderer.targets[0].depth_view?1u:0u,
        renderer.targets[1].depth && renderer.targets[1].depth_view?1u:0u,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    std::fflush(stdout);
  }
};
}
