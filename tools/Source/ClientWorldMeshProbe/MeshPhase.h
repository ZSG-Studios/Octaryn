#pragma once
#include "Probe.h"
namespace mesh_probe {
// Explicit qualification waits through the private CPU allocation phase.
// Production frame/spare-time progress never calls this helper or waits.
template<class Jobs> bool finish_mesh_allocations(WorldRenderer& r,Jobs& jobs) {
  bool pending=false;
  for(std::size_t slot=0;slot<Jobs::Capacity;++slot)if(jobs.resources(slot).allocating) {
    require(jobs.wait(slot,1000000000ull),"explicit mesh output allocation completion");pending=true;
  }
  return !pending || jobs.progress(r,1000.0);
}
template<class Jobs> bool progress_mesh_phase(WorldRenderer& r,Jobs& jobs) {
  return jobs.progress(r,1000.0) && finish_mesh_allocations(r,jobs);
}
}
