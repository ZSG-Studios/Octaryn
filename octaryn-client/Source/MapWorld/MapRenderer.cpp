#include "MapRendererInternal.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include "MapMeshOptimization.h"
#include "MapForwardGeometry.h"
#include "../VirtualGeometry/WorldGeometry.h"
#include "../VirtualGeometry/WorldGeometryRay.h"
#include <cstdio>
#include <chrono>
#include <filesystem>

namespace octaryn::client::rendering {
namespace {
bool create_buffer(rhi::IDevice* device,const void* data,std::uint64_t size,unsigned element_size,
    bool acceleration,bool indices,Slang::ComPtr<rhi::IBuffer>& target) {
  rhi::BufferDesc desc{};
  desc.usage=indices?rhi::BufferUsage::IndexBuffer:rhi::BufferUsage::ShaderResource;
  desc.defaultState=indices?rhi::ResourceState::IndexBuffer:rhi::ResourceState::ShaderResource;
  if(acceleration)desc.usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
  desc.size=size;desc.elementSize=element_size;
  return SLANG_SUCCEEDED(device->createBuffer(desc,data,target.writeRef()));
}
}
bool create_map_pipelines(MapRenderer& map,rhi::Format color_format,rhi::Format depth_format,const char* shader_path) {
  Slang::ComPtr<rhi::IShaderProgram> program;
  rhi::RenderPipelineDesc desc{};
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  desc.depthStencil.format=depth_format;desc.depthStencil.depthTestEnable=true;
  const char* forward_entries[]={"forward_vertex_main","forward_main"};
  if(!create_rhi_program(map.device.get(),shader_path,forward_entries,2,program))return false;
  rhi::ColorTargetDesc forward{};
  forward.format=color_format;forward.enableBlend=true;
  forward.color.srcFactor=rhi::BlendFactor::SrcAlpha;
  forward.alpha.srcFactor=rhi::BlendFactor::One;
  forward.color.dstFactor=forward.alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  desc.program=program;desc.targets=&forward;desc.targetCount=1;
  desc.depthStencil.depthFunc=rhi::ComparisonFunc::Less;
  desc.depthStencil.depthWriteEnable=false;
  // The shader applies each material's double-sided rule.
  desc.rasterizer.cullMode=rhi::CullMode::None;
  if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.forward_pipeline.writeRef())))return false;
  forward.color.dstFactor=forward.alpha.dstFactor=rhi::BlendFactor::One;
  if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.additive_pipeline.writeRef())))return false;
  if(map.device->hasFeature(rhi::Feature::RayQuery)) {
    const char* rt_path="octaryn-client/Shaders/Map/WorldMapRT.slang";
    if(!create_rhi_program(map.device.get(),rt_path,forward_entries,2,program))return false;
    desc.program=program;
    forward.color.dstFactor=forward.alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
    if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.forward_rt_pipeline.writeRef())))return false;
    forward.color.dstFactor=forward.alpha.dstFactor=rhi::BlendFactor::One;
    if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.additive_rt_pipeline.writeRef())))return false;
  }
  return true;
}
const MapModel& map_model(const MapRenderer& map) {return map.model;}
void release_map_cpu_geometry(MapRenderer* map) {
  if(!map)return;
  std::vector<MapVertex>().swap(map->model.vertices);
  std::vector<std::uint32_t>().swap(map->model.indices);
}
MapMemoryStats map_memory_stats(const MapRenderer& map) {
  MapMemoryStats stats{};stats.textures=map.texture_bytes;
  for(auto* buffer:{map.vertices.get(),map.indices.get(),map.raster_indices.get(),map.ray_primitives.get(),
      map.indirect_primitives.get(),map.lod_indices.get(),map.meshlets.get(),
      map.meshlet_vertices.get(),map.meshlet_triangles.get()})
    if(buffer)stats.geometry+=buffer->getDesc().size;
  if(map.geometry)stats.geometry+=map.geometry->gpu_bytes();
  else if(!map.geometry_cache.path.empty())stats.reserved+=virtual_geometry::map_geometry_reservation(map.geometry_cache);
  const auto ray_bytes=map.geometry_ray?map.geometry_ray->gpu_bytes():0;
  const auto ray_reservation=virtual_geometry::geometry_ray_reservation(map.geometry_cache);
  stats.acceleration+=ray_bytes;
  if(ray_reservation>ray_bytes)stats.reserved+=ray_reservation-ray_bytes;
  if(!map.geometry_instances.empty())stats.reserved+=virtual_geometry::geometry_ray_instance_reservation(
      map.geometry_cache,map.geometry_instances.size());
  for(auto* acceleration:{map.blas.get(),map.tlas.get(),map.uncompacted_blas.get()})
    if(acceleration)stats.acceleration+=acceleration->getDesc().size;
  for(auto* buffer:{map.blas_scratch.get(),map.tlas_scratch.get(),map.instances.get()})
    if(buffer)stats.scratch+=buffer->getDesc().size;
  return stats;
}
void destroy_map_renderer(MapRenderer* map) {if(map)finish_map_ray_scene(*map);delete map;}
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path,bool world_geometry,
    MapLoadProgressFn progress,void* progress_user) {
  if(!device || !glb_path || !shader_path)return nullptr;
  auto map=std::make_unique<MapRenderer>();map->device=device;
  struct CpuLease {
    MapLoadProgressFn progress;void* user;bool active{};
    ~CpuLease() {
      if(active && progress)try {progress("Finishing current operation",false,user);}catch(...) {}
    }
  } lease{progress,progress_user};
  const auto feedback=[&](const char* label,bool cpu_only=false) {
    lease.active=cpu_only;
    if(progress)progress(label,cpu_only,progress_user);
  };
  const auto acceleration=device->hasFeature(rhi::Feature::AccelerationStructure);
  auto stage_start=std::chrono::steady_clock::now();
  const auto stage=[&](const char* name) {
    const auto now=std::chrono::steady_clock::now();
    std::printf("map_startup stage=%s elapsed_ms=%.2f\n",name,
        std::chrono::duration<double,std::milli>(now-stage_start).count());
    std::fflush(stdout);stage_start=now;
  };
  map->texture_cache_directory=std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path));
  map->texture_cache_directory+=".textures";
  std::string error;
  feedback("Reading map",true);
  if(!load_map_model(std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path)),map->model,error)) {
    std::fprintf(stderr,"map_model_load_failed: %s\n",error.c_str());return nullptr;
  }
  stage("load");
  feedback("Optimizing geometry",true);
  if(!optimize_map_mesh(map->model,error)) {
    std::fprintf(stderr,"map_mesh_optimization_failed: %s\n",error.c_str());return nullptr;
  }
  stage("optimize");
  map->vertex_count=static_cast<std::uint32_t>(map->model.vertices.size());
  map->index_count=static_cast<std::uint32_t>(map->model.indices.size());
  map->ray_supported=acceleration;
  for(const auto& primitive:map->model.primitives)
    if(primitive.material.alpha_mode==MapAlphaMode::Blend && primitive.material.double_sided)
      map->any_double_sided_blend=true;
  feedback("Uploading textures");
  if(!upload_map_images(*map,progress,progress_user)) {std::fprintf(stderr,"map_renderer_failed stage=images\n");return nullptr;}
  stage("images");
  feedback("Uploading materials");
  if(!upload_map_materials(*map)) {std::fprintf(stderr,"map_renderer_failed stage=materials\n");return nullptr;}
  stage("materials");
  feedback("Preparing geometry",true);
  if(world_geometry && !virtual_geometry::prepare_map_geometry(std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path)),
      map->model,map->geometry_cache,error)) {
    std::fprintf(stderr,"map_geometry_cook_failed: %s\n",error.c_str());return nullptr;
  }
  MapForwardGeometry forward;
  if(world_geometry && !build_map_forward_geometry(map->model,forward,error)) {
    std::fprintf(stderr,"map_forward_prepare_failed: %s\n",error.c_str());return nullptr;
  }
  const auto& vertices=world_geometry?forward.vertices:map->model.vertices;
  const auto& indices=world_geometry?forward.indices:map->model.indices;
  feedback("Uploading geometry");
  if(!indices.empty() && (!create_buffer(device,vertices.data(),vertices.size()*sizeof(MapVertex),
          sizeof(MapVertex),!world_geometry && acceleration,false,map->vertices) ||
      !create_buffer(device,indices.data(),indices.size()*sizeof(std::uint32_t),
          sizeof(std::uint32_t),!world_geometry && acceleration,false,map->indices) ||
      !create_buffer(device,indices.data(),indices.size()*sizeof(std::uint32_t),
          sizeof(std::uint32_t),false,true,map->raster_indices)))return nullptr;
  map->forward_first_indices=std::move(forward.first_indices);
  if(world_geometry)std::printf("map_forward_geometry triangles=%zu vertices=%zu indexed_bytes=%zu opaque_indexed=0\n",
      indices.size()/3,vertices.size(),vertices.size()*sizeof(MapVertex)+indices.size()*8);
  stage("geometry_upload");
  feedback("Preparing graphics");
  if((!world_geometry || !indices.empty()) && !create_map_pipelines(*map,color_format,depth_format,shader_path)) {
    std::fprintf(stderr,"map_renderer_failed stage=pipelines\n");return nullptr;
  }
  stage("pipelines");
  std::printf("map_renderer_loaded vertices=%zu indices=%zu primitives=%zu images=%zu double_sided_blend=%u ray=%u\n",
      map->model.vertices.size(),map->model.indices.size()/3,map->model.primitives.size(),
      map->model.images.size(),map->any_double_sided_blend?1u:0u,acceleration?1u:0u);
  return map.release();
}
}
