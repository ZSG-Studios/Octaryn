#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
namespace octaryn::client::rendering {
bool world_local_clusters(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.restir;
  const unsigned capacity=std::clamp(s.settings.tile_capacity,1u,256u);
  const unsigned tiles[4]={(s.width+15)/16,(s.height+15)/16,capacity,16};
  const auto count=std::uint64_t(tiles[0])*tiles[1];
  if(!s.tile_counts || !s.tile_lights || capacity!=s.tile_capacity) {
    rhi::BufferDesc d{};d.elementSize=4;
    d.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination;
    d.defaultState=rhi::ResourceState::ShaderResource;d.size=count*4;
    if(!world_rhi_ok(r.device->createBuffer(d,nullptr,s.tile_counts.writeRef())))return false;
    d.size=count*capacity*4;
    if(!world_rhi_ok(r.device->createBuffer(d,nullptr,s.tile_lights.writeRef())))return false;
    s.tile_capacity=capacity;
  }
  commands->clearBuffer(s.tile_counts);commands->setBufferState(s.tile_counts,rhi::ResourceState::UnorderedAccess);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(s.tile_pipeline);bool ok=root!=nullptr;
  if(ok) {
    rhi::ShaderCursor c(root);const auto view=temporal_view(r.temporal.camera,int(s.width),int(s.height));
    const unsigned extent[4]={s.width,s.height,unsigned(r.frames),unsigned(s.lights.size())};
    ok=world_rhi_ok(c["localLights"].setBinding(rhi::Binding(s.light_buffer)))&&
      world_rhi_ok(c["localTileCounts"].setBinding(rhi::Binding(s.tile_counts)))&&
      world_rhi_ok(c["localTileLights"].setBinding(rhi::Binding(s.tile_lights)))&&
      world_rhi_ok(c["localTileSettings"].setData(tiles,sizeof(tiles)))&&
      world_rhi_ok(c["extent"].setData(extent,sizeof(extent)))&&
      world_rhi_ok(c["eye"].setData(view.position.data(),sizeof(view.position)))&&
      world_rhi_ok(c["clusterRight"].setData(view.right.data(),sizeof(view.right)))&&
      world_rhi_ok(c["clusterUp"].setData(view.up.data(),sizeof(view.up)))&&
      world_rhi_ok(c["clusterForward"].setData(view.forward.data(),sizeof(view.forward)))&&
      world_rhi_ok(c["clusterProjection"].setData(view.projection.data(),sizeof(view.projection)));
  }
  if(ok)pass->dispatchCompute((unsigned(s.lights.size())+63)/64,1,1);
  pass->end();
  commands->setBufferState(s.tile_counts,rhi::ResourceState::ShaderResource);
  commands->setBufferState(s.tile_lights,rhi::ResourceState::ShaderResource);return ok;
}
}
