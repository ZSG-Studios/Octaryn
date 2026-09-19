#include "MapRendererInternal.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include <cstdio>
#include <filesystem>

namespace octaryn::client::rendering {
namespace {
bool create_buffer(rhi::IDevice* device,const void* data,std::uint64_t size,unsigned element_size,
    bool acceleration,Slang::ComPtr<rhi::IBuffer>& target) {
  rhi::BufferDesc desc{};
  desc.usage=rhi::BufferUsage::ShaderResource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  if(acceleration)desc.usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
  desc.size=size;desc.elementSize=element_size;
  return SLANG_SUCCEEDED(device->createBuffer(desc,data,target.writeRef()));
}
bool create_samplers(MapRenderer& map) {
  rhi::SamplerDesc desc{};
  desc.minFilter=desc.magFilter=desc.mipFilter=rhi::TextureFilteringMode::Linear;
  desc.addressU=desc.addressV=desc.addressW=rhi::TextureAddressingMode::Wrap;
  desc.maxLOD=0;
  return SLANG_SUCCEEDED(map.device->createSampler(desc,map.sampler.writeRef()));
}
bool create_white_texture(MapRenderer& map) {
  rhi::TextureDesc desc{};
  desc.size={1,1,1};desc.format=rhi::Format::RGBA8UnormSrgb;desc.mipCount=1;
  desc.sampleCount=1;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
  desc.memoryType=rhi::MemoryType::DeviceLocal;
  const std::uint32_t white=0xFFFFFFFFu;
  const rhi::SubresourceData data{&white,4,4};
  if(SLANG_FAILED(map.device->createTexture(desc,&data,map.white.writeRef())))return false;
  return SLANG_SUCCEEDED(map.white->getDefaultView(map.white_view.writeRef()));
}
bool create_pipelines(MapRenderer& map,rhi::Format color_format,rhi::Format depth_format,const char* shader_path) {
  const char* gbuffer_entries[]={"vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(map.device.get(),shader_path,gbuffer_entries,2,program))return false;
  rhi::ColorTargetDesc targets[4]{};
  for(unsigned i=0;i<4;++i)targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};
  desc.program=program;desc.targets=targets;desc.targetCount=4;
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  desc.rasterizer.cullMode=rhi::CullMode::Back;
  desc.depthStencil.format=depth_format;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  if(SLANG_FAILED(map.device->createRenderPipeline(desc,map.gbuffer_pipeline.writeRef())))return false;
  const char* forward_entries[]={"forward_vertex_main","forward_main"};
  if(!create_rhi_program(map.device.get(),shader_path,forward_entries,2,program))return false;
  rhi::ColorTargetDesc forward{};
  forward.format=color_format;forward.enableBlend=true;
  forward.color.srcFactor=forward.alpha.srcFactor=rhi::BlendFactor::SrcAlpha;
  forward.color.dstFactor=forward.alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  desc.program=program;desc.targets=&forward;desc.targetCount=1;
  desc.depthStencil.depthFunc=rhi::ComparisonFunc::Less;
  desc.depthStencil.depthWriteEnable=false;
  // Blended double-sided geometry needs both faces to sort against itself.
  desc.rasterizer.cullMode=map.any_double_sided_blend?rhi::CullMode::None:rhi::CullMode::Back;
  return SLANG_SUCCEEDED(map.device->createRenderPipeline(desc,map.forward_pipeline.writeRef()));
}
bool fill_ray_records(MapRenderer& map) {
  std::vector<MapRayMaterial> materials(map.model.primitives.size());
  std::vector<std::uint32_t> triangle_primitives;
  for(size_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    auto& record=materials[index];
    for(size_t k=0;k<4;++k)record.base_color[k]=primitive.material.base_color[k];
    record.alpha_cutoff=primitive.material.alpha_cutoff;
    record.alpha_mode=static_cast<std::uint32_t>(primitive.material.alpha_mode);
    triangle_primitives.resize(primitive.first_index/3+primitive.index_count/3,
        static_cast<std::uint32_t>(index));
  }
  return create_buffer(map.device.get(),materials.data(),materials.size()*sizeof(MapRayMaterial),
      sizeof(MapRayMaterial),false,map.ray_primitives) &&
      create_buffer(map.device.get(),triangle_primitives.data(),triangle_primitives.size()*4,4,false,
          map.ray_triangle_primitives);
}
}
const MapModel& map_model(const MapRenderer& map) {return map.model;}
void destroy_map_renderer(MapRenderer* map) {delete map;}
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path) {
  if(!device || !glb_path || !shader_path)return nullptr;
  auto map=std::make_unique<MapRenderer>();map->device=device;
  std::string error;
  if(!load_map_model(std::filesystem::path(reinterpret_cast<const char8_t*>(glb_path)),map->model,error)) {
    std::fprintf(stderr,"map_model_load_failed: %s\n",error.c_str());return nullptr;
  }
  const auto acceleration=device->hasFeature(rhi::Feature::AccelerationStructure);
  map->ray_supported=acceleration;
  if(!create_buffer(device,map->model.vertices.data(),map->model.vertices.size()*sizeof(MapVertex),
          sizeof(MapVertex),acceleration,map->vertices) ||
      !create_buffer(device,map->model.indices.data(),map->model.indices.size()*sizeof(std::uint32_t),
          sizeof(std::uint32_t),acceleration,map->indices))return nullptr;
  for(const auto& primitive:map->model.primitives)
    if(primitive.material.alpha_mode==MapAlphaMode::Blend && primitive.material.double_sided)
      map->any_double_sided_blend=true;
  if(!create_samplers(*map) || !create_white_texture(*map) || !upload_map_images(*map) ||
      !create_pipelines(*map,color_format,depth_format,shader_path))return nullptr;
  if(acceleration && !fill_ray_records(*map))return nullptr;
  std::printf("map_renderer_loaded vertices=%zu indices=%zu primitives=%zu images=%zu double_sided_blend=%u ray=%u\n",
      map->model.vertices.size(),map->model.indices.size()/3,map->model.primitives.size(),
      map->model.images.size(),map->any_double_sided_blend?1u:0u,acceleration?1u:0u);
  return map.release();
}
}
