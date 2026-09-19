#pragma once
#include "WorldRendererInternal.h"

namespace octaryn::client::rendering {
// Publish before either volume traces, so recursive sampling sees both wakes.
inline void world_ddgi_lighting_changes(WorldRenderer& r) {
  auto& changes=r.lighting_changes;
  const auto each_volume=[&](auto visit) {
    if(r.ddgi.available && r.ddgi.initialized)visit(r.ddgi);
    if(r.ddgi.fine_volume && r.ddgi.fine_volume->available && r.ddgi.fine_volume->initialized)
      visit(*r.ddgi.fine_volume);
  };
  if(changes.light_revision!=r.local_lighting.light_revision) {
    changes.local.update(r.local_lighting.lights,[&](const LightInfluence& bounds,bool removed) {
      each_volume([&](DDGISystem& s) {
        // Source generations retain geometric history and request four observations.
        ddgi_invalidate(s,bounds.minimum,bounds.maximum,s.config.spacing,true,removed,true);
        s.burst_frames=std::max(s.burst_frames,8u);
      });
    });
    changes.light_revision=r.local_lighting.light_revision;
  }
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  const auto change=changes.environment.update(r.sky,r.lighting,seconds);
  if(change==EnvironmentChange::Discontinuous)each_volume([&](DDGISystem& s) {
    // Only an abrupt jump requests a bounded global response. Ordinary day/night
    // drift stays on the age-based cadence, which is stable and needs no
    // per-frame global wake that would merely re-inject ray noise.
    ddgi_environment_changed(s,true);
  });
}
}
