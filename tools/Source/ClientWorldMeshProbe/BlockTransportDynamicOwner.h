#pragma once
#include "BlockTransportSetup.h"
#include "LocalLight.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/DynamicReceivers.h"
#include <cmath>

namespace mesh_probe {
inline void block_transport_dynamic_owner_case(Fixture& fixture) {
  auto& r=fixture.renderer;
  using Pixel=std::array<float,4>;
  struct Node {Pixel minimum,maximum;std::array<unsigned,4> links;};
  DynamicReceivers owner;
  require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/DynamicReceivers.slang",
      "main",owner.pipeline),"BT dynamic owner prepared production pipeline");
  require(initialize_dynamic_receivers(owner,r),"BT dynamic owner resource allocation");
  require(world_ray_scene_usable(r),"BT dynamic owner needs real published scene");
  r.block_gi.active=false;
  WorldLocalLight light;light.position_range={11,7,11,24};light.color_intensity={.9f,.4f,.15f,20};light.axis_v_type[3]=0;
  const Node node{{11,7,11,1},{11,7,11,0},{0,0,0,1}};
  r.local_lighting.light_buffer=buffer(r,&light,sizeof(light),sizeof(light),rhi::BufferUsage::ShaderResource);
  r.block_gi.light_tree=buffer(r,&node,sizeof(node),sizeof(node),rhi::BufferUsage::ShaderResource);
  r.block_gi.light_nodes=1;r.sky.light_direction_sky[0]=0;r.sky.light_direction_sky[1]=-1;
  r.sky.light_direction_sky[2]=0;r.sky.light_direction_sky[3]=0;
  r.lighting.sun_strength=1;r.lighting.ambient_strength=0;r.lighting_settings.shadow_distance=64;
  const DynamicReceiver input{{11,6,11,0},{0,1,0,0},{0x4f574e45u,1,0,0}};
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT dynamic owner frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT dynamic owner encoder");
  require(prepare_dynamic_receivers(owner,r,commands,std::span<const DynamicReceiver>(&input,1)),
      "BT dynamic owner direct preparation with inactive GI cache");
  require(owner.active && owner.count==1 && r.block_gi.lookup_empty_surface && r.block_gi.lookup_empty_value,
      "BT dynamic owner skipped direct or failed to bind bounded empty lookup");
  auto command=commands->finish();require(bool(command),"BT dynamic owner finish");
  require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BT dynamic owner bounded completion");
  Pixel indirect{},direct{};std::array<unsigned,4> counters{};
  checked(r.device->readBuffer(owner.values,0,sizeof(indirect),indirect.data()),"BT inactive cache indirect readback");
  checked(r.device->readBuffer(owner.direct,0,sizeof(direct),direct.data()),"BT independent owner direct readback");
  checked(r.device->readBuffer(owner.counters,0,sizeof(counters),counters.data()),"BT owner ray count readback");
  const double source=20*std::pow(1-1./std::pow(24.,4),2)/3.14159265358979323846;
  const double color[3]={.9,.4,.15};
  for(unsigned c=0;c<3;++c) {
    require(indirect[c]==0 && std::isfinite(direct[c]) && std::abs(direct[c]-source*color[c])<2e-5,
        "BT inactive cache lost direct lighting or exposed uncertified indirect");
  }
  require(indirect[3]==0 && direct[3]==1 && counters[0]==0 && counters[1]==1 && counters[2]==1,
      "BT inactive cache direct/indirect work contract");
  require(r.debug.errors.load()==0,"BT dynamic owner graphics validation errors");
  block_transport_complete(r,start);
  std::puts("block_transport_dynamic_owner=passed hardware=1 production_prepare=1 inactive_cache=1 usable_scene=1 empty_lookup=1 zero_indirect=1 local_direct=1 solar_visibility=1 bounded_queries=1 validation_errors=0");
}
}
