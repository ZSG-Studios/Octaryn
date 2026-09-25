#include "MapRendererInternal.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include "MapMeshOptimization.h"
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
bool create_pipelines(MapRenderer& map,rhi::Format color_format,rhi::Format depth_format,const char* shader_path) {
  const char* gbuffer_entries[]={"vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(map.device.get(),shader_path,gbuffer_entries,2,program))return false;
  rhi::ColorTargetDesc targets[world_gbuffer_formats.size()]{};
  for(unsigned i=0;i<world_gbuffer_formats.size();++i)targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};
  desc.program=program;desc.targets=targets;desc.targetCount=world_gbuffer_count;
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.depthStencil.format=depth_format;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.gbuffer_pipeline.writeRef())))return false;
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
  // The shader applies each material's double-sided rule in both passes.
  desc.rasterizer.cullMode=rhi::CullMode::None;
  if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.forward_pipeline.writeRef())))return false;
  if(map.device->hasFeature(rhi::Feature::RayQuery)) {
    const char* rt_path="octaryn-client/Shaders/Map/WorldMapRT.slang";
    if(!create_rhi_program(map.device.get(),rt_path,forward_entries,2,program))return false;
    desc.program=program;
    if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.forward_rt_pipeline.writeRef())))return false;
  }
  const char* shadow_path="octaryn-client/Shaders/Map/MapShadow.slang";
  const char* shadow_entries[]={"vertex_main","fragment_main"};
  if(!create_rhi_program(map.device.get(),shadow_path,shadow_entries,2,program))return false;
  rhi::RenderPipelineDesc shadow{};shadow.program=program;
  shadow.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  shadow.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  shadow.rasterizer.cullMode=rhi::CullMode::None;
  shadow.depthStencil.format=depth_format;shadow.depthStencil.depthTestEnable=true;
  shadow.depthStencil.depthWriteEnable=true;shadow.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  if(SLANG_FAILED(map.device->createRenderPipeline(shadow,map.shadow_pipeline.writeRef())))return false;
  const char* local_shadow_path="octaryn-client/Shaders/Map/MapLocalShadow.slang";
  if(!create_rhi_program(map.device.get(),local_shadow_path,shadow_entries,2,program))return false;
  shadow.program=program;
  if(SLANG_FAILED(map.device->createRenderPipeline(shadow,map.local_shadow_pipeline.writeRef())))return false;
  return true;
}
}
const MapModel& map_model(const MapRenderer& map) {return map.model;}
void destroy_map_renderer(MapRenderer* map) {if(map)finish_map_ray_scene(*map);delete map;}
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path) {
  if(!device || !glb_path || !shader_path)return nullptr;
  auto stage_start=std::chrono::steady_clock::now();
  const auto stage=[&](const char* name) {
    const auto now=std::chrono::steady_clock::now();
    std::printf("map_startup stage=%s elapsed_ms=%.2f\n",name,
        std::chrono::duration<double,std::milli>(now-stage_start).count());
    std::fflush(stdout);stage_start=now;
  };
  auto map=std::make_unique<MapRenderer>();map->device=device;
  map->texture_cache_directory=std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path));
  map->texture_cache_directory+=".textures";
  std::string error;
  if(!load_map_model(std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path)),map->model,error)) {
    std::fprintf(stderr,"map_model_load_failed: %s\n",error.c_str());return nullptr;
  }
  stage("load");
  const auto acceleration=device->hasFeature(rhi::Feature::AccelerationStructure);
  if(!optimize_map_mesh(map->model,error)) {
    std::fprintf(stderr,"map_mesh_optimization_failed: %s\n",error.c_str());return nullptr;
  }
  stage("optimize");
  map->ray_supported=acceleration;
  if(!create_buffer(device,map->model.vertices.data(),map->model.vertices.size()*sizeof(MapVertex),
          sizeof(MapVertex),acceleration,false,map->vertices) ||
      !create_buffer(device,map->model.indices.data(),map->model.indices.size()*sizeof(std::uint32_t),
          sizeof(std::uint32_t),acceleration,false,map->indices) ||
      !create_buffer(device,map->model.indices.data(),map->model.indices.size()*sizeof(std::uint32_t),
          sizeof(std::uint32_t),false,true,map->raster_indices))return nullptr;
  stage("geometry_upload");
  for(const auto& primitive:map->model.primitives)
    if(primitive.material.alpha_mode==MapAlphaMode::Blend && primitive.material.double_sided)
      map->any_double_sided_blend=true;
  if(!upload_map_images(*map)) {std::fprintf(stderr,"map_renderer_failed stage=images\n");return nullptr;}
  stage("images");
  if(!upload_map_materials(*map)) {std::fprintf(stderr,"map_renderer_failed stage=materials\n");return nullptr;}
  stage("materials");
  if(!create_pipelines(*map,color_format,depth_format,shader_path)) {
    std::fprintf(stderr,"map_renderer_failed stage=pipelines\n");return nullptr;
  }
  stage("pipelines");
  std::printf("map_renderer_loaded vertices=%zu indices=%zu primitives=%zu images=%zu double_sided_blend=%u ray=%u\n",
      map->model.vertices.size(),map->model.indices.size()/3,map->model.primitives.size(),
      map->model.images.size(),map->any_double_sided_blend?1u:0u,acceleration?1u:0u);
  return map.release();
}
}
