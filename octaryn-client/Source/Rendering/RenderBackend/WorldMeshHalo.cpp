#include "WorldRendererInternal.h"
#include "WorldStream.h"
namespace octaryn::client::rendering {
namespace {
bool neighborhood_ready(const WorldRenderer& r,std::pair<std::int32_t,std::int32_t> coordinate) {
  for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
    if(dx==0 && dz==0)continue;
    const auto neighbor=std::make_pair(coordinate.first+dx,coordinate.second+dz);
    if(std::abs(std::int64_t(neighbor.first)-r.center_x)>r.radius ||
        std::abs(std::int64_t(neighbor.second)-r.center_z)>r.radius)continue;
    if(!r.sources.contains(neighbor))return false;
  }
  return true;
}
}
bool world_mesh_take_pending(WorldRenderer& r,std::pair<std::int32_t,std::int32_t>& coordinate) {
  for(auto it=r.dirty_urgent.begin();it!=r.dirty_urgent.end();) {
    const bool resident=r.sources.contains(*it);
    if(!resident || !r.dirty.contains(*it)) {
      if(!resident) r.dirty.erase(*it);
      it=r.dirty_urgent.erase(it);
    } else ++it;
  }
  // An urgent coordinate already in flight must not prevent the other slot
  // from servicing ordinary work. Its reinvalidation remains in both sets.
  for(auto* candidates:{&r.dirty_urgent,&r.dirty}) {
    auto selected=candidates->end();std::int64_t nearest{};
    for(auto it=candidates->begin();it!=candidates->end();) {
      if(!r.sources.contains(*it)) {it=candidates->erase(it);continue;}
      if(r.halo_jobs && r.halo_jobs->contains(*it)) {++it;continue;}
      const auto dx=std::int64_t(it->first)-r.center_x,dz=std::int64_t(it->second)-r.center_z;
      const auto distance=dx*dx+dz*dz;
      if(selected==candidates->end() || distance<nearest) {
        // Coalesce loading boundaries until the expected neighborhood arrives.
        // Urgent edits/unloads never wait; current window edges are known absent.
        if(candidates==&r.dirty && !neighborhood_ready(r,*it)) {++it;continue;}
        selected=it;nearest=distance;
      }
      ++it;
    }
    if(selected==candidates->end())continue;
    coordinate=*selected;r.dirty.erase(coordinate);r.dirty_urgent.erase(coordinate);
    return true;
  }
  return false;
}
bool world_mesh_refresh_one(WorldRenderer& r) {
  if(!r.halo_jobs) {
    if(r.dirty.empty())return true;
    r.halo_jobs=std::make_unique<WorldHaloJobs>();
  }
  return r.halo_jobs->pump(r);
}
}
