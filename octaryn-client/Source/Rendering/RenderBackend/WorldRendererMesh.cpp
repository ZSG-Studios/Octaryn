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

bool world_renderer_mesh(WorldRenderer& r,const world_presentation::StreamColumn& source,WorldColumnGpu& output) {
  // Explicit qualification uses this blocking completion path.
  if(!r.qualification_mesh)r.qualification_mesh=std::make_unique<WorldMeshJob>();
  auto& job=*r.qualification_mesh;
  if(!job.start(r,source))return false;
  bool complete=false;
  while(!complete)if(!job.wait() || !job.poll(r,output,complete))return false;
  return true;
}
}
