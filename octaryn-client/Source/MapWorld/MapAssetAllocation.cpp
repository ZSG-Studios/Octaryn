#include "MapAssetBuildInternal.h"
#include "MapRayResources.h"
#include "../Threading/BackgroundThread.h"
#include <chrono>
#include <cstdio>

namespace octaryn::client::rendering {
namespace {
using Clock=std::chrono::steady_clock;
struct Allocation {
  MapRendererBuild& build;
  double maximum{};
  unsigned resources{};
  template<class Operation> bool call(const char* name,Operation operation) {
    if(build.cancelled.load(std::memory_order_relaxed))return false;
    const auto start=Clock::now();const auto result=operation();
    const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    maximum=std::max(maximum,ms);++resources;
    if(SLANG_FAILED(result))std::fprintf(stderr,"map_resource_failed operation=%s result=0x%08x\n",name,unsigned(result));
    return SLANG_SUCCEEDED(result);
  }
  bool buffer(Slang::ComPtr<rhi::IBuffer>& output,std::uint64_t bytes,unsigned stride,rhi::BufferUsage usage,
      rhi::ResourceState state=rhi::ResourceState::ShaderResource) {
    if(!bytes)return true;
    rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.usage=usage;
    desc.defaultState=state;
    return call("buffer",[&]{return build.map->device->createBuffer(desc,nullptr,output.writeRef());});
  }
  bool run() {
    auto& map=*build.map;
    auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    if(map.ray_supported)usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
    const auto index_usage=rhi::BufferUsage::IndexBuffer|rhi::BufferUsage::CopyDestination;
    const auto shader_usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    const auto& forward=build.prepared.forward;
    if(!buffer(map.vertices,forward.vertices.size()*sizeof(MapVertex),sizeof(MapVertex),usage) ||
        !buffer(map.indices,forward.indices.size()*4,4,usage) ||
        !buffer(map.raster_indices,forward.indices.size()*4,4,index_usage,rhi::ResourceState::IndexBuffer) ||
        !buffer(map.ray_primitives,map.model.primitives.size()*sizeof(MapRayMaterial),sizeof(MapRayMaterial),shader_usage))return false;
    if(map.lod_pixel_error>0 && !buffer(map.lod_indices,
        (map.model.indices.size()+build.prepared.lods.indices.size())*4,4,index_usage,rhi::ResourceState::IndexBuffer))return false;
    if(map.indirect_enabled && !buffer(map.indirect_primitives,build.indirect.size()*sizeof(MapIndirectPrimitive),
        sizeof(MapIndirectPrimitive),shader_usage|rhi::BufferUsage::CopySource))return false;
    const auto& mesh=build.prepared.meshlets;
    if(map.meshlet_count && (!buffer(map.meshlets,mesh.records.size()*sizeof(MapMeshlet),sizeof(MapMeshlet),shader_usage) ||
        !buffer(map.meshlet_vertices,mesh.vertices.size()*4,4,shader_usage) ||
        !buffer(map.meshlet_triangles,mesh.triangles.size()*4,4,shader_usage)))return false;
    for(size_t i=0;i<map.texture_resources.size();++i) {
      auto& resource=*map.texture_resources[i];
      if(resource.ready)continue;
      const auto& cached=build.prepared.images.textures[i].texture;
      if(cached.levels.empty())return false;
      rhi::TextureDesc desc{};desc.size={cached.levels[0].width,cached.levels[0].height,1};
      desc.format=cached.compressed?(cached.srgb?rhi::Format::BC7UnormSrgb:rhi::Format::BC7Unorm):
          (cached.srgb?rhi::Format::RGBA8UnormSrgb:rhi::Format::RGBA8Unorm);
      desc.mipCount=unsigned(cached.levels.size());desc.sampleCount=1;
      desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
      desc.defaultState=rhi::ResourceState::ShaderResource;
      if(!call("texture",[&]{return map.device->createTexture(desc,nullptr,resource.texture.writeRef());}) ||
          !call("texture_view",[&]{return resource.texture->getDefaultView(resource.view.writeRef());}))return false;
      map.textures[i]=resource.texture;map.texture_views[i]=resource.view;
    }
    return !build.allocate_ray || allocate_map_ray_resources(map,&build.cancelled);
  }
};
}
bool start_map_resource_allocation(MapRendererBuild& build) {
  // Only this owner touches pool membership. The single worker owns unpublished
  // resources until future completion; backend initialization uses private recording.
  // Bindless handles and samplers remain on the publication owner.
  auto& map=*build.map;
  for(size_t i=0;i<build.prepared.images.textures.size();++i) {
    auto& prepared=build.prepared.images.textures[i];
    const auto key=map_texture_pool_key(build.prepared.texture_cache,prepared.key,prepared.texture.compressed);
    auto resource=prepared.resident?prepared.resident->resource:build.pool->textures[key].lock();
    if(resource && !resource->ready)return false;
    if(!resource) {
      resource=std::make_shared<MapTextureResource>();
      for(const auto& level:prepared.texture.levels)resource->bytes+=level.blocks.size();
      const auto& cached=prepared.texture;if(cached.levels.empty())return false;
      resource->cache_directory=build.prepared.texture_cache;resource->content_key=prepared.key;
      resource->validated_cache=prepared.cached;
      resource->metadata={cached.srgb,cached.compressed,cached.opaque,
          cached.levels.front().width,cached.levels.front().height,unsigned(cached.levels.size()),resource->bytes};
      build.pool->textures[key]=resource;build.created.push_back(resource);
    } else {
      map.textures[i]=resource->texture;map.texture_views[i]=resource->view;
      prepared.texture.levels.clear();
      // Restore weak accounting membership even if an earlier owner evicted it.
      build.pool->textures[key]=resource;
    }
    map.texture_resources[i]=resource;map.texture_bytes+=resource->bytes;
    prepared.resident.reset();
  }
  build.allocation=std::async(std::launch::async,[&build] {
    threading::set_background_thread_priority("map_allocation");
    Allocation allocation{build};const auto start=Clock::now();bool result=false;
    try {result=allocation.run();}catch(...) {result=false;}
    build.allocation_progress.allocation_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    build.allocation_progress.allocation_max_call_ms=allocation.maximum;
    build.allocation_progress.allocated_resources=allocation.resources;
    return result;
  });
  return true;
}
}
