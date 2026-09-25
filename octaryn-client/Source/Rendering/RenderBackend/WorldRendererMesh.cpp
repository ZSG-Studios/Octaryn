#include "WorldRendererInternal.h"
#include "WorldMeshJob.h"
namespace octaryn::client::rendering {
bool world_renderer_progress_delivery(WorldRenderer& r) {
  return !r.delivery_jobs || r.delivery_jobs->progress(r);
}
bool open_world_renderer_stream(WorldRenderer* r,world_presentation::WorldStream& stream) {
  if(!r)return false;
  if(!r->delivery_jobs)r->delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  return r->delivery_jobs->pump(*r,stream);
}
bool open_world_renderer_stream_progress(WorldRenderer* r,world_presentation::WorldStream& stream,double budget_ms) {
  if(!r)return false;
  if(!(budget_ms>0))return true;
  const auto start=std::chrono::steady_clock::now();
  // A bounded first share prevents mesh/halo work from starving ready BLAS.
  if(!world_ray_progress(*r,budget_ms*.25))return false;
  const auto ray_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  if(ray_ms>=budget_ms)return true;
  if(!r->delivery_jobs)r->delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  if(!r->delivery_jobs->prefetch(*r,stream,(budget_ms-ray_ms)*.5))return false;
  const auto used=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  return used>=budget_ms || !r->halo_jobs || r->halo_jobs->progress(*r,budget_ms-used);
}

bool world_renderer_mesh(WorldRenderer& r,const world_presentation::StreamColumn& source,WorldColumnGpu& output) {
  // Explicit qualification uses this blocking completion path.
  if(!r.qualification_mesh)r.qualification_mesh=std::make_unique<WorldMeshJob>();
  auto& job=*r.qualification_mesh;
  if(!job.start(r,source))return false;
  bool complete=false;
  while(!complete) {
    if(!job.wait(frame_fence_timeout_ms()*1000000ull)) {r.status="fence_timeout";return false;}
    if(!job.poll(r,output,complete))return false;
  }
  return true;
}
}
