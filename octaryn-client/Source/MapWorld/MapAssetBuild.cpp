#include "MapAssetBuildInternal.h"
#include "MapUploadBudget.h"
#include "../VirtualGeometry/WorldGeometryRay.h"
#include "RhiShader.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include <cstring>

namespace octaryn::client::rendering {
namespace {
bool pump_buffer(MapRendererBuild& build,rhi::ICommandEncoder* commands,Slang::ComPtr<rhi::IBuffer>& target,
    const void* source,std::uint64_t size,rhi::ResourceState final_state,
    std::uint64_t& budget,bool& done,const void* extra=nullptr,std::uint64_t split=0) {
  done=false;
  if(!target)return false;
  auto bytes=map_buffer_upload_bytes(size-build.offset,budget);
  const auto* data=static_cast<const std::uint8_t*>(source);
  if(extra && build.offset>=split)data=static_cast<const std::uint8_t*>(extra)+build.offset-split;
  else {data+=build.offset;if(extra)bytes=std::min(bytes,split-build.offset);}
  if(!bytes)return true;
  const auto upload_start=std::chrono::steady_clock::now();
  if(SLANG_FAILED(commands->uploadBufferData(target,build.offset,bytes,data)))return false;
  build.progress.max_upload_call_ms=std::max(build.progress.max_upload_call_ms,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-upload_start).count());
  build.progress.commands_recorded=true;
  budget-=bytes;build.offset+=bytes;
  if(build.offset==size) {commands->setBufferState(target,final_state);build.offset=0;done=true;}
  return true;
}
MapBuildStatus poll(MapRendererBuild& build) {
  if(build.status!=MapBuildStatus::Waiting)return build.status;
  std::uint64_t completed{};
  if(SLANG_FAILED(build.submitted_fence->getCurrentValue(&completed)) || completed==std::numeric_limits<std::uint64_t>::max())
    return build.status=MapBuildStatus::Failed;
  if(completed>=build.submitted_value) {
    if(!build.cancelled && !build.created.empty()) {
      for(auto& resource:build.created)resource->ready=true;
      ++build.pool->ready_generation;
    }
    build.status=MapBuildStatus::Ready;
    release_map_cpu_geometry(build.map.get());
  }
  return build.status;
}
}
std::uint64_t map_prepared_geometry_bytes(const PreparedMapAsset& asset) {
  const auto& model=asset.model;
  std::uint64_t bytes=asset.forward.vertices.size()*sizeof(MapVertex)+asset.forward.indices.size()*2*sizeof(std::uint32_t)+
      model.primitives.size()*sizeof(MapRayMaterial);
  bytes+=virtual_geometry::map_geometry_reservation(asset.geometry_cache)+
      virtual_geometry::geometry_ray_reservation(asset.geometry_cache);
  return bytes;
}
MapRendererBuild* begin_map_renderer_build(rhi::IDevice* device,rhi::Format color,rhi::Format depth,
    PreparedMapAsset&& prepared,std::shared_ptr<MapTexturePool> pool,bool ray_required) {
  if(!device || prepared.model.primitives.empty())return nullptr;
  auto build=std::make_unique<MapRendererBuild>();build->prepared=std::move(prepared);
  build->allocate_ray=false;
  build->pool=pool?std::move(pool):create_map_texture_pool();build->color=color;build->depth=depth;
  build->map=std::make_unique<MapRenderer>();auto& map=*build->map;map.device=device;
  map.sampler_cache=build->pool->samplers;
  map.geometry_cache=std::move(build->prepared.geometry_cache);
  map.model=std::move(build->prepared.model);map.texture_cache_directory=build->prepared.source;map.texture_cache_directory+=".textures";
  map.forward_first_indices=std::move(build->prepared.forward.first_indices);
  map.vertex_count=static_cast<std::uint32_t>(map.model.vertices.size());map.index_count=static_cast<std::uint32_t>(map.model.indices.size());
  map.material_texture_slots=std::move(build->prepared.images.slots);
  const auto images=build->prepared.images.textures.size();map.textures.resize(images);map.texture_views.resize(images);map.texture_resources.resize(images);
  map.ray_supported=device->hasFeature(rhi::Feature::AccelerationStructure);
  if(!bind_map_cached_pipelines(*build->pool,map,color,depth)) {
    std::fprintf(stderr,"map_asset_build_failed reason=pipelines_not_prewarmed\n");return nullptr;
  }
  for(const auto& primitive:map.model.primitives)
    if(primitive.material.alpha_mode==MapAlphaMode::Blend && primitive.material.double_sided)map.any_double_sided_blend=true;
  if(!start_map_resource_allocation(*build))return nullptr;
  return build.release();
}
MapBuildStatus pump_map_renderer_build(MapRendererBuild* pointer,rhi::ICommandEncoder* commands,
    std::uint64_t byte_budget,double milliseconds) {
  if(!pointer)return MapBuildStatus::Failed;
  auto& build=*pointer;build.progress.uploaded_this_pump=0;build.progress.cpu_ms=0;
  build.progress.commands_recorded=false;
  if(poll(build)!=MapBuildStatus::Progress)return build.status;
  if(!commands || byte_budget<512 || milliseconds<=0)return build.status;
  if(!build.allocation_ready) {
    if(build.allocation.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return build.status;
    const bool allocated=build.allocation.get();build.allocation_ready=true;
    build.progress=build.allocation_progress;
    std::printf("map_resource_allocation cpu_ms=%.3f maximum_call_ms=%.3f resources=%u cancelled=%u\n",
        build.progress.allocation_ms,build.progress.allocation_max_call_ms,build.progress.allocated_resources,
        build.cancelled.load()?1u:0u);
    if(!allocated && !build.cancelled)return build.status=MapBuildStatus::Failed;
  }
  if(build.cancelled) {
    if(!build.progress.total_uploaded) {release_map_cpu_geometry(build.map.get());return build.status=MapBuildStatus::Ready;}
    return build.status=MapBuildStatus::NeedsSubmission;
  }
  const auto start=std::chrono::steady_clock::now();auto& map=*build.map;
  const auto fail=[&] {std::fprintf(stderr,"map_asset_upload_failed stage=%u\n",build.stage);return build.status=MapBuildStatus::Failed;};
  byte_budget=std::min(byte_budget,map_upload_frame_bytes);
  auto budget=byte_budget;
  struct Record {
    MapRendererBuild& build;std::uint64_t initial;std::uint64_t& remaining;std::chrono::steady_clock::time_point start;
    ~Record() {
      build.progress.uploaded_this_pump=initial-remaining;build.progress.total_uploaded+=initial-remaining;
      build.progress.cpu_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
  } record{build,byte_budget,budget,start};
  while(budget>=512 && std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<milliseconds) {
    bool done=false;
    if(build.stage==0) {
      const auto& forward=build.prepared.forward;
      if(forward.indices.empty()) {++build.stage;continue;}
      const bool vertices=build.geometry==0,raster=build.geometry>=2,lod=build.geometry==3;
      auto& target=vertices?map.vertices:lod?map.lod_indices:raster?map.raster_indices:map.indices;
      if(!pump_buffer(build,commands,target,vertices?static_cast<const void*>(forward.vertices.data()):forward.indices.data(),
          vertices?forward.vertices.size()*sizeof(MapVertex):(forward.indices.size()+(lod?build.prepared.lods.indices.size():0))*4,
          raster?rhi::ResourceState::IndexBuffer:rhi::ResourceState::ShaderResource,budget,done,
          lod?build.prepared.lods.indices.data():nullptr,forward.indices.size()*4))return fail();
      if(done && ++build.geometry==(map.lod_pixel_error>0?4u:3u)) {build.prepared.forward={};++build.stage;}
    } else if(build.stage==1) {
      if(build.image==build.prepared.images.textures.size()) {++build.stage;continue;}
      bool progressed=false;if(!pump_map_image(build,commands,budget,progressed))return fail();
      if(!progressed)break;
    } else if(build.stage==2) {
      if(build.materials.size()<map.model.primitives.size()) {
        const auto material_start=std::chrono::steady_clock::now();
        if(!prepare_map_materials(map,build.materials,1))return fail();
        build.progress.material_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-material_start).count();
        build.progress.material_records=unsigned(build.materials.size());continue;
      }
      if(!pump_buffer(build,commands,map.ray_primitives,build.materials.data(),build.materials.size()*sizeof(MapRayMaterial),
          rhi::ResourceState::ShaderResource,budget,done))return fail();
      if(done) {build.materials.clear();++build.stage;}
    } else if(build.stage==3) {
      if(!map.indirect_enabled) {++build.stage;continue;}
      if(!pump_buffer(build,commands,map.indirect_primitives,build.indirect.data(),build.indirect.size()*sizeof(MapIndirectPrimitive),
          rhi::ResourceState::ShaderResource,budget,done))return fail();
      if(done)++build.stage;
    } else if(build.stage==4) {
      const auto& mesh=build.prepared.meshlets;
      if(!map.meshlet_count) {++build.stage;continue;}
      const bool records=build.meshlet_buffer==0,vertices=build.meshlet_buffer==1;
      auto& target=records?map.meshlets:vertices?map.meshlet_vertices:map.meshlet_triangles;
      const void* source=records?static_cast<const void*>(mesh.records.data()):vertices?mesh.vertices.data():mesh.triangles.data();
      const auto size=records?mesh.records.size()*sizeof(MapMeshlet):vertices?mesh.vertices.size()*4:mesh.triangles.size()*4;
      if(!pump_buffer(build,commands,target,source,size,
          rhi::ResourceState::ShaderResource,budget,done))return fail();
      if(done && ++build.meshlet_buffer==3) {build.prepared.meshlets={};++build.stage;}
    } else {
      return build.status=MapBuildStatus::NeedsSubmission;
    }
  }
  return build.status;
}
MapBuildProgress map_renderer_build_progress(const MapRendererBuild* build) {
  if(!build)return {};
  auto result=build->progress;
  if(!build->allocation_ready) {result.allocation_pending=true;return result;}
  if(!build->map)return result;
  const auto& map=*build->map;result.pending_bytes=0;
  if(build->stage==0) {
    const auto& forward=build->prepared.forward;
    if(build->geometry==0)result.pending_bytes+=forward.vertices.size()*sizeof(MapVertex);
    if(build->geometry<=1)result.pending_bytes+=forward.indices.size()*4;
    if(build->geometry<=2)result.pending_bytes+=forward.indices.size()*4;
    if(map.lod_pixel_error>0)result.pending_bytes+=(map.model.indices.size()+build->prepared.lods.indices.size())*4;
    result.pending_bytes-=build->offset;
  }
  for(size_t i=build->image;i<build->prepared.images.textures.size();++i) {
    const auto& texture=build->prepared.images.textures[i].texture;
    for(const auto& level:texture.levels)result.pending_bytes+=level.blocks.size();
    if(i==build->image && !texture.levels.empty()) {
      const auto& level=texture.levels[build->mip];
      result.pending_bytes-=std::uint64_t(build->row/(texture.compressed?4:1))*(texture.compressed?(level.width+3)/4*16:level.width*4);
    }
  }
  if(build->stage<=2)result.pending_bytes+=map.model.primitives.size()*sizeof(MapRayMaterial)-(build->stage==2?build->offset:0);
  if(map.indirect_enabled && build->stage<=3)result.pending_bytes+=build->indirect.size()*sizeof(MapIndirectPrimitive)-(build->stage==3?build->offset:0);
  if(map.meshlet_enabled && build->stage<=4) {
    const auto& mesh=build->prepared.meshlets;
    if(build->stage<4 || build->meshlet_buffer==0)result.pending_bytes+=mesh.records.size()*sizeof(MapMeshlet);
    if(build->stage<4 || build->meshlet_buffer<=1)result.pending_bytes+=mesh.vertices.size()*4;
    result.pending_bytes+=mesh.triangles.size()*4;
    if(build->stage==4)result.pending_bytes-=build->offset;
  }
  const auto memory=map_memory_stats(map);result.gpu_allocated=memory.geometry+memory.textures+memory.acceleration+memory.scratch;
  return result;
}
void map_renderer_build_submitted(MapRendererBuild* build,rhi::IFence* fence,std::uint64_t value) {
  if(!build || !fence || build->status!=MapBuildStatus::NeedsSubmission)return;
  build->submitted_fence=fence;build->submitted_value=value;build->status=MapBuildStatus::Waiting;
}
MapRenderer* take_map_renderer_build(MapRendererBuild* build) {
  if(!build || poll(*build)!=MapBuildStatus::Ready)return nullptr;
  return build->map.release();
}
void cancel_map_renderer_build(MapRendererBuild* build) {
  if(!build || build->cancelled.exchange(true))return;
  // Incomplete textures must never become a shared-cache hit after cancellation.
  for(auto entry=build->pool->textures.begin();entry!=build->pool->textures.end();) {
    const auto resource=entry->second.lock();
    if(resource && std::find(build->created.begin(),build->created.end(),resource)!=build->created.end())
      entry=build->pool->textures.erase(entry);
    else ++entry;
  }
}
void destroy_map_renderer_build(MapRendererBuild* build) {
  if(!build)return;
  build->cancelled=true;
  if(build->allocation.valid())build->allocation.wait();
  delete build;
}
}
