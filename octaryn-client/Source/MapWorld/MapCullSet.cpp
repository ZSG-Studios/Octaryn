#include "MapCullSet.h"
#include "MapIndirectData.h"
#include "MapRendererInternal.h"
#include "MapVisibility.h"
#include "WorldRendererInternal.h"
#include "RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace octaryn::client::rendering {
bool create_map_cull_set(rhi::IDevice* device,MapCullSet& set) {
  if(!device)return false;
  rhi::BufferDesc desc{};
  desc.elementSize=sizeof(MapIndirectPrimitive);
  desc.size=MapCullSet::kSlots*MapCullSet::kSlotPrimitives*sizeof(MapIndirectPrimitive);
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,set.primitives.writeRef())))return false;
  desc.elementSize=sizeof(std::uint32_t);
  desc.size=MapCullSet::kSlots*MapCullSet::kSlotArguments*sizeof(std::uint32_t);
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::IndirectArgument;
  desc.defaultState=rhi::ResourceState::IndirectArgument;
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,set.arguments_a.writeRef())) ||
      SLANG_FAILED(device->createBuffer(desc,nullptr,set.arguments_b.writeRef())))return false;
  desc.size=MapCullSet::kSlots*MapCullSet::kSlotPrimitives*sizeof(std::uint32_t);
  desc.usage=rhi::BufferUsage::UnorderedAccess;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,set.flags.writeRef())))return false;
  set.members.assign(MapCullSet::kSlots,nullptr);
  return create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Map/MapCull.slang","cull_main",set.pipeline);
}
bool sync_map_cull_set(MapCullSet& set,rhi::ICommandEncoder* commands,
    std::span<const std::shared_ptr<MapRenderer>> maps) {
  // Membership rebuilds from resident slots each frame, so departed maps free
  // their slots automatically without dereferencing stale pointers.
  std::fill(set.members.begin(),set.members.end(),nullptr);
  for(const auto& map:maps)
    if(map && map->indirect_enabled && map->cull_slot>=0)set.members[map->cull_slot]=map.get();
  for(const auto& map:maps) {
    if(!map || !map->indirect_enabled || map->cull_slot>=0)continue;
    const auto primitives=map->model.primitives.size();
    if(!primitives)continue;
    // Wide maps own a contiguous run of slots; global entry indexing makes a
    // run transparent to both the cull dispatch and the indirect draw.
    const auto needed=static_cast<std::uint32_t>(primitives+MapCullSet::kSlotPrimitives-1)/MapCullSet::kSlotPrimitives;
    std::uint32_t run=0,start=0,found=MapCullSet::kSlots;
    for(std::uint32_t slot=0;slot<MapCullSet::kSlots;++slot) {
      if(set.members[slot]) {run=0;continue;}
      if(!run)start=slot;
      if(++run==needed) {found=start;break;}
    }
    if(found==MapCullSet::kSlots)continue; // Table full: direct per-primitive fallback.
    commands->setBufferState(map->indirect_primitives,rhi::ResourceState::CopySource);
    commands->copyBuffer(set.primitives,found*MapCullSet::kSlotPrimitives*sizeof(MapIndirectPrimitive),
        map->indirect_primitives,0,primitives*sizeof(MapIndirectPrimitive));
    commands->setBufferState(map->indirect_primitives,rhi::ResourceState::ShaderResource);
    for(std::uint32_t slot=found;slot<found+needed;++slot)set.members[slot]=map.get();
    map->cull_slot=static_cast<std::int32_t>(found);
  }
  set.covered=0;set.occlusion=false;
  for(std::uint32_t slot=0;slot<MapCullSet::kSlots;++slot) {
    const auto* member=set.members[slot];
    if(!member)continue;
    set.covered=(slot+1)*MapCullSet::kSlotPrimitives;
    set.occlusion=set.occlusion || member->occlusion_enabled;
  }
  return true;
}
bool dispatch_map_cull(MapCullSet& set,rhi::ICommandEncoder* commands,const WorldCamera& eye,
    WorldRenderer& r,unsigned phase) {
  if(!set.covered || (phase && !set.occlusion))return true;
  const auto visibility=map_visibility_camera(eye,r);
  const float position[4]={eye.x,eye.y,eye.z,0};
  const float forward[4]={r.view_uniforms[12],r.view_uniforms[13],r.view_uniforms[14],0};
  // lod_pixel_error comes from a process-wide env var, so members all agree.
  float lod_error=0;
  for(const auto* member:set.members)if(member) {lod_error=member->lod_pixel_error;break;}
  const float lod_settings[4]={float(r.render_height())/(2*std::tan(eye.vertical_fov*.5f)),lod_error,0,0};
  const unsigned settings[4]={set.covered,
      std::getenv("OCTARYN_CLIENT_MAP_DISABLE_CULLING")==nullptr?1u:0u,phase,0};
  // right, up (with jitter), forward and projection rows matching map_vertex.
  float transform[4][4];
  for(unsigned row=0;row<4;++row)std::copy_n(r.view_uniforms.begin()+4+row*4,4,transform[row]);
  const bool occlusion=set.occlusion && r.hiz.pyramid && (phase!=0 || r.hiz.valid);
  const float hiz[4]={float(r.hiz.width),float(r.hiz.height),float(r.hiz.mips),occlusion?1.f:0.f};
  auto& arguments=phase?set.arguments_b:set.arguments_a;
  commands->setBufferState(arguments,rhi::ResourceState::UnorderedAccess);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(set.pipeline);bool okay=root!=nullptr;
  if(okay) {
    rhi::ShaderCursor cursor(root);
    okay=SLANG_SUCCEEDED(cursor["mapCullPrimitives"].setBinding(rhi::Binding(set.primitives))) &&
        SLANG_SUCCEEDED(cursor["mapDrawArguments"].setBinding(rhi::Binding(arguments))) &&
        SLANG_SUCCEEDED(cursor["mapCullPlanes"].setData(visibility.relative_frustum_planes,sizeof(visibility.relative_frustum_planes))) &&
        SLANG_SUCCEEDED(cursor["mapCullEye"].setData(position,sizeof(position))) &&
        SLANG_SUCCEEDED(cursor["mapLodForward"].setData(forward,sizeof(forward))) &&
        SLANG_SUCCEEDED(cursor["mapLodSettings"].setData(lod_settings,sizeof(lod_settings))) &&
        SLANG_SUCCEEDED(cursor["mapCullSettings"].setData(settings,sizeof(settings))) &&
        SLANG_SUCCEEDED(cursor["mapCullTransform"].setData(transform,sizeof(transform))) &&
        SLANG_SUCCEEDED(cursor["mapCullHiz"].setData(hiz,sizeof(hiz)));
    if(okay && set.occlusion && r.hiz.pyramid)
      okay=SLANG_SUCCEEDED(cursor["mapHizPyramid"].setBinding(rhi::Binding(r.hiz.pyramid))) &&
          SLANG_SUCCEEDED(cursor["mapHizSampler"].setBinding(rhi::Binding(r.hiz.sampler)));
    if(okay)okay=SLANG_SUCCEEDED(cursor["mapCullFlags"].setBinding(rhi::Binding(set.flags)));
  }
  if(okay)pass->dispatchCompute((settings[0]+63)/64,1,1);
  pass->end();commands->setBufferState(arguments,rhi::ResourceState::IndirectArgument);
  return okay;
}
}
