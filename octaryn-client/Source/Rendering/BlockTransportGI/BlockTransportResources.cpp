#include "BlockTransportGI.h"
#include "../Atlas/WorldAtlas.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <memory>

namespace octaryn::client::rendering {
namespace {
bool buffer(WorldRenderer& r,Slang::ComPtr<rhi::IBuffer>& result,std::uint64_t count,
    unsigned stride,const char* label,bool readback=false) {
  if(result)return true;
  rhi::BufferDesc desc{};desc.size=count*stride;desc.elementSize=stride;desc.label=label;
  desc.usage=readback?rhi::BufferUsage::CopyDestination:
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|
      rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  desc.defaultState=readback?rhi::ResourceState::CopyDestination:rhi::ResourceState::ShaderResource;
  desc.memoryType=readback?rhi::MemoryType::ReadBack:rhi::MemoryType::DeviceLocal;
  return world_rhi_ok(r.device->createBuffer(desc,nullptr,result.writeRef()));
}
bool pipelines(WorldRenderer& r) {
  auto& s=r.block_gi;
  struct Pipeline {Slang::ComPtr<rhi::IComputePipeline>* target;const char* path;const char* entry="main";};
  const Pipeline pipelines[]{
      {std::addressof(s.clear),"Clear"},{std::addressof(s.evict),"Evict"},
      {std::addressof(s.admit),"Admit"},{std::addressof(s.trace),"Trace"},
      {std::addressof(s.admit_contributors),"AdmitContributors"},{std::addressof(s.resolve_contributors),"ResolveContributors"},
      {std::addressof(s.direct),"Direct"},{std::addressof(s.bounce),"Bounce"},
      {std::addressof(s.select_count),"Select","count_main"},
      {std::addressof(s.select_prefix),"Select","prefix_main"},
      {std::addressof(s.select_rows),"Select","select_main"}};
  for(const auto& pipeline:pipelines) {
    if(*pipeline.target)continue;
    const auto path=std::string("octaryn-client/Shaders/BlockTransportGI/")+pipeline.path+".slang";
    if(!create_rhi_compute_pipeline(r.device,path.c_str(),pipeline.entry,*pipeline.target))return false;
  }
  return true;
}
void memory_stats(WorldRenderer& r) {
  auto& s=r.block_gi;s.stats.gpu_bytes=0;s.stats.surface_key_bytes=0;
  for(auto* value:{s.surfaces.get(),s.links.get(),s.candidates.get(),s.direct_values.get(),s.environment.get(),
      s.output.get(),s.counters.get(),s.light_tree.get(),s.lookup_empty_surface.get(),s.lookup_empty_value.get(),
      s.work_rows.get(),s.work_blocks.get(),s.schedule.get()})
    if(value)s.stats.gpu_bytes+=value->getDesc().size;
  for(const auto& value:s.orders)if(value)s.stats.gpu_bytes+=value->getDesc().size;
  for(const auto& value:s.readback)if(value)s.stats.gpu_bytes+=value->getDesc().size;
  s.stats.gpu_bytes+=world_atlas_plant_mask_bytes(r.atlas);
  if(r.gi_mode==GiMode::BlockTransport)for(const auto& target:r.targets) {
    if(!target.hdr.block_transport || !target.hdr.gbuffer[5])continue;
    const auto& size=target.hdr.gbuffer[5]->getDesc().size;
    s.stats.surface_key_bytes+=std::uint64_t(size.width)*size.height*16;
  }
  s.stats.total_gpu_bytes=s.stats.gpu_bytes+s.stats.surface_key_bytes;
}
}
bool prepare_block_transport_gi(WorldRenderer& r) {
  if(r.gi_mode!=GiMode::BlockTransport) {r.block_gi.active=false;memory_stats(r);return true;}
  if(!r.device->hasFeature(rhi::Feature::RayQuery)) {r.status="block_transport_requires_ray_query";return false;}
  auto& s=r.block_gi;const bool first=!s.surfaces;
  constexpr std::uint64_t limit=128ull*1024*1024;
  constexpr std::uint64_t fixed=std::uint64_t(BlockTransportCapacity)*sizeof(BlockTransportSurface)+
      std::uint64_t(BlockTransportCapacity)*BlockTransportLinks*sizeof(BlockTransportLink)+
      std::uint64_t(BlockTransportRows)*BlockTransportLinks*sizeof(BlockTransportCandidate)+
      std::uint64_t(BlockTransportCapacity)*16*6+131071ull*48+48*3+80+WorldAtlasPlantMaskBytes+
      BlockTransportRows*sizeof(BlockTransportWorkRow)+
      ((BlockTransportCapacity+BlockTransportSelectGroup-1)/BlockTransportSelectGroup)*16+64*3;
  static_assert(fixed<=limit);
  if(!prepare_world_atlas_plant_masks(r.atlas)) {r.status="block_transport_plant_masks";return false;}
  if(!pipelines(r) ||
      !buffer(r,s.surfaces,BlockTransportCapacity,sizeof(BlockTransportSurface),"block_transport_surfaces") ||
      !buffer(r,s.links,std::uint64_t(BlockTransportCapacity)*BlockTransportLinks,sizeof(BlockTransportLink),"block_transport_links") ||
      !buffer(r,s.candidates,std::uint64_t(BlockTransportRows)*BlockTransportLinks,sizeof(BlockTransportCandidate),"block_transport_candidates") ||
      !buffer(r,s.direct_values,BlockTransportCapacity,16,"block_transport_direct") ||
      !buffer(r,s.environment,BlockTransportCapacity,16,"block_transport_environment") ||
      !buffer(r,s.output,BlockTransportCapacity,16,"block_transport_indirect") ||
      !buffer(r,s.work_rows,BlockTransportRows,sizeof(BlockTransportWorkRow),"block_transport_selected_rows") ||
      !buffer(r,s.work_blocks,(BlockTransportCapacity+BlockTransportSelectGroup-1)/BlockTransportSelectGroup,
          16,"block_transport_selection_blocks") ||
      !buffer(r,s.schedule,16,4,"block_transport_schedule") ||
      !buffer(r,s.counters,12,4,"block_transport_counters") ||
      !buffer(r,s.light_tree,131071,48,"block_transport_light_tree"))return false;
  for(auto& value:s.orders)if(!buffer(r,value,BlockTransportCapacity,16,"block_transport_bounce"))return false;
  for(auto& value:s.readback)if(!buffer(r,value,28,4,"block_transport_statistics",true))return false;
  memory_stats(r);
  if(s.stats.gpu_bytes>limit) {r.status="block_transport_cache_memory_limit";return false;}
  if(first)std::printf("block_transport_resources capacity=%u links=%u rows_per_frame=%u world_face_budget=%u world_radius=%u gpu_bytes=%llu surface_key_bytes=%llu total_gpu_bytes=%llu cache_cap_bytes=%llu\n",
      BlockTransportCapacity,BlockTransportLinks,BlockTransportRows,BlockAdmissionFaces,BlockAdmissionRadius,
      static_cast<unsigned long long>(s.stats.gpu_bytes),static_cast<unsigned long long>(s.stats.surface_key_bytes),
      static_cast<unsigned long long>(s.stats.total_gpu_bytes),static_cast<unsigned long long>(limit));
  return true;
}
bool bind_block_transport_lookup(WorldRenderer& r,rhi::IShaderObject* root) {
  if(!root)return false;
  rhi::ShaderCursor c(root);auto field=c["btLookupInfo"];if(!field.isValid())return true;
  auto& s=r.block_gi;
  const bool active=r.gi_mode==GiMode::BlockTransport && s.active;
  if(!active && (!s.lookup_empty_surface || !s.lookup_empty_value)) {
    const std::array<std::uint32_t,16> zero{};
    auto create=[&](Slang::ComPtr<rhi::IBuffer>& value,unsigned size) {
      if(value)return true;
      rhi::BufferDesc desc{};desc.size=size;desc.elementSize=size;desc.label="block_transport_empty_lookup";
      desc.defaultState=rhi::ResourceState::ShaderResource;desc.usage=rhi::BufferUsage::ShaderResource;
      return world_rhi_ok(r.device->createBuffer(desc,zero.data(),value.writeRef()));
    };
    if(!create(s.lookup_empty_surface,64) || !create(s.lookup_empty_value,16))return false;
    memory_stats(r);
  }
  const std::array<unsigned,4> info{active?1u:0u,s.pending?s.pending_epoch:s.epoch,
      s.pending?s.pending_radiance_epoch:s.radiance_epoch,BlockTransportCapacity};
  auto bind=[&](const char* name,rhi::IBuffer* value) {
    auto target=c[name];return !target.isValid() || world_rhi_ok(target.setBinding(rhi::Binding(value)));
  };
  return world_rhi_ok(field.setData(info.data(),sizeof(info))) &&
      bind("btLookupSurfaces",active?s.surfaces.get():s.lookup_empty_surface.get()) &&
      bind("btLookupDirect",active?s.direct_values.get():s.lookup_empty_value.get()) &&
      bind("btLookupIndirect",active?s.output.get():s.lookup_empty_value.get()) &&
      bind("btLookupEnvironment",active?s.environment.get():s.lookup_empty_value.get());
}
}
