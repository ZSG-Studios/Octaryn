#include "DynamicReceivers.h"
#include "BlockTransportGI.h"
#include "BlockTransportLighting.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <vector>

namespace octaryn::client::rendering {
namespace {
bool allocate(WorldRenderer& r,Slang::ComPtr<rhi::IBuffer>& target,unsigned count,unsigned stride,bool writable) {
  if(target)return true;
  const std::vector<std::uint8_t> zero(std::size_t(count)*stride);
  rhi::BufferDesc desc{};desc.size=zero.size();desc.elementSize=stride;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  if(writable)desc.usage|=rhi::BufferUsage::UnorderedAccess;
  desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="block_transport_dynamic_receivers";
  return world_rhi_ok(r.device->createBuffer(desc,zero.data(),target.writeRef()));
}
}
bool initialize_dynamic_receivers(DynamicReceivers& receiver,WorldRenderer& r) {
  if(!allocate(r,receiver.empty,1,16,false))return false;
  if(r.gi_mode==GiMode::BlockTransport) {
    if(!receiver.pipeline && !create_rhi_compute_pipeline(r.device,
        "octaryn-client/Shaders/BlockTransportGI/DynamicReceivers.slang","main",receiver.pipeline))return false;
    if(!allocate(r,receiver.input,DynamicReceiverCapacity,sizeof(DynamicReceiver),false) ||
        !allocate(r,receiver.previous,DynamicReceiverCapacity,sizeof(DynamicReceiver),true) ||
        !allocate(r,receiver.values,DynamicReceiverCapacity,16,true) ||
        !allocate(r,receiver.direct,DynamicReceiverCapacity,16,true) ||
        !allocate(r,receiver.state,DynamicReceiverCapacity,8,true) ||
        !allocate(r,receiver.counters,4,4,true))return false;
  }
  receiver.gpu_bytes=0;
  for(auto* value:{receiver.input.get(),receiver.previous.get(),receiver.values.get(),receiver.direct.get(),
      receiver.state.get(),receiver.counters.get(),receiver.empty.get()})
    if(value)receiver.gpu_bytes+=value->getDesc().size;
  return true;
}
bool prepare_dynamic_receivers(DynamicReceivers& receiver,WorldRenderer& r,rhi::ICommandEncoder* commands,
    std::span<const DynamicReceiver> inputs) {
  receiver.active=false;receiver.count=0;receiver.samples=0;
  const std::array<unsigned,4> zero{};
  if(receiver.counters && commands && !world_rhi_ok(commands->uploadBufferData(receiver.counters,0,sizeof(zero),zero.data())))return false;
  if(r.gi_mode!=GiMode::BlockTransport || inputs.empty() || !r.ray_enabled || !world_ray_scene_usable(r))return true;
  if(!commands || inputs.size()>DynamicReceiverCapacity) {r.status="dynamic_receiver_capacity";return false;}
  if(!receiver.pipeline || !receiver.values || !receiver.direct || !receiver.state || !receiver.counters) {
    r.status="dynamic_receivers_not_initialized";return false;
  }
  std::vector<DynamicReceiver> current(inputs.begin(),inputs.end());
  for(auto& value:current) {value.identity[2]=r.block_gi.pending_epoch;value.identity[3]=r.block_gi.pending_radiance_epoch;}
  if(!world_rhi_ok(commands->uploadBufferData(receiver.input,0,current.size()*sizeof(DynamicReceiver),current.data())))return false;
  receiver.count=static_cast<unsigned>(inputs.size());receiver.samples=std::min(16u,DynamicReceiverRayBudget/receiver.count);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(receiver.pipeline);bool ok=root && bind_world_atlas(r.atlas,root) &&
      world_ray_bind(r,root) && bind_block_transport_lookup(r,root);
  if(ok) {
    const rhi::ShaderCursor c(root);const std::array<unsigned,4> work{receiver.count,receiver.samples,r.block_gi.pending_player_revision,0};
    const auto lighting=block_transport_lighting(
        {-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength},
        {r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0});
    ok=world_rhi_ok(c["btReceivers"].setBinding(receiver.input)) &&
        world_rhi_ok(c["btPreviousReceivers"].setBinding(receiver.previous)) &&
        world_rhi_ok(c["btReceiverValues"].setBinding(receiver.values)) &&
        world_rhi_ok(c["btReceiverDirect"].setBinding(receiver.direct)) &&
        world_rhi_ok(c["btReceiverState"].setBinding(receiver.state)) &&
        world_rhi_ok(c["btReceiverCounters"].setBinding(receiver.counters)) &&
        world_rhi_ok(c["localLights"].setBinding(r.local_lighting.light_buffer)) &&
        world_rhi_ok(c["giLightTree"].setBinding(r.block_gi.light_tree)) &&
        world_rhi_ok(c["giLightNodeCount"].setData(&r.block_gi.light_nodes,sizeof(r.block_gi.light_nodes))) &&
        world_rhi_ok(c["btReceiverWork"].setData(work.data(),sizeof(work))) &&
        world_rhi_ok(c["btCoverageMin"].setData(r.block_gi.coverage_min.data(),16)) &&
        world_rhi_ok(c["btCoverageMax"].setData(r.block_gi.coverage_max.data(),16)) &&
        world_rhi_ok(c["btSun"].setData(lighting.sun.data(),16)) &&
        world_rhi_ok(c["btSky"].setData(lighting.sky.data(),16)) &&
        world_rhi_ok(c["btReceiverShadowRange"].setData(&r.lighting_settings.shadow_distance,sizeof(float)));
  }
  if(ok)pass->dispatchCompute((receiver.count+63)/64,1,1);pass->end();
  if(!ok)return false;
  commands->globalBarrier();commands->setBufferState(receiver.values,rhi::ResourceState::ShaderResource);
  commands->setBufferState(receiver.direct,rhi::ResourceState::ShaderResource);
  receiver.active=true;return true;
}
bool bind_dynamic_receivers(DynamicReceivers& receiver,WorldRenderer& r,rhi::IShaderObject* root) {
  const rhi::ShaderCursor c(root);auto field=c["btDynamicInfo"];if(!field.isValid())return true;
  if(!receiver.active && !allocate(r,receiver.empty,1,16,false))return false;
  const std::array<unsigned,4> info{receiver.active?1u:0u,receiver.count,r.gi_mode==GiMode::BlockTransport?1u:0u,0};
  return world_rhi_ok(field.setData(info.data(),sizeof(info))) &&
      world_rhi_ok(c["btDynamicValues"].setBinding(receiver.active?receiver.values.get():receiver.empty.get())) &&
      world_rhi_ok(c["btDynamicDirect"].setBinding(receiver.active?receiver.direct.get():receiver.empty.get()));
}
}
