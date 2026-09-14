#include "WorldRendererInternal.h"
#include "WorldMeshJob.h"
namespace octaryn::client::rendering {
bool world_renderer_mesh(WorldRenderer& r,const world_presentation::StreamColumn& source,WorldColumnGpu& output) {
  // Initial stream delivery remains synchronous: query publication and its
  // visible mesh must agree before camera and targeting run on this frame.
  if(!r.delivery_mesh)r.delivery_mesh=std::make_unique<WorldMeshJob>();
  auto& job=*r.delivery_mesh;
  if(!job.start(r,source))return false;
  bool complete=false;
  while(!complete)if(!job.wait() || !job.poll(r,output,complete))return false;
  return true;
}
}
