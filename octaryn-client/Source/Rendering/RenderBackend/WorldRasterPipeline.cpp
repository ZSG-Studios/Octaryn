#include "WorldRasterPipeline.h"
#include "WorldHdr.h"
#include "RhiShader.h"

namespace octaryn::client::rendering {
bool create_world_raster_pipelines(rhi::IDevice* device,
    Slang::ComPtr<rhi::IRenderPipeline>& opaque,
    Slang::ComPtr<rhi::IRenderPipeline>& sprite,
    Slang::ComPtr<rhi::IRenderPipeline>& lava,
    Slang::ComPtr<rhi::IRenderPipeline>& transparent) {
  const char* entries[]={"vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(device,"octaryn-client/Shaders/Voxel/WorldRaster.slang",entries,2,program)) return false;
  rhi::ColorTargetDesc targets[4]{};
  for(unsigned i=0;i<4;++i) targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc pipeline{};pipeline.program=program;pipeline.targets=targets;pipeline.targetCount=4;
  pipeline.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  pipeline.depthStencil.format=rhi::Format::D32Float;
  pipeline.depthStencil.depthTestEnable=true;pipeline.depthStencil.depthWriteEnable=true;
  pipeline.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  // The original G-buffer culled opaque backs but kept crossed sprites two-sided.
  pipeline.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  pipeline.rasterizer.cullMode=rhi::CullMode::Back;
  if(SLANG_FAILED(device->createRenderPipeline(pipeline,opaque.writeRef()))) return false;
  pipeline.rasterizer.cullMode=rhi::CullMode::None;
  if(SLANG_FAILED(device->createRenderPipeline(pipeline,sprite.writeRef()))) return false;
  entries[0]="forward_vertex_main";entries[1]="forward_main";
  if(!create_rhi_program(device,"octaryn-client/Shaders/Voxel/WorldRaster.slang",entries,2,program)) return false;
  pipeline.program=program;pipeline.targetCount=1;targets[0].format=rhi::Format::RGBA16Float;
  pipeline.depthStencil.depthFunc=rhi::ComparisonFunc::Less;
  if(SLANG_FAILED(device->createRenderPipeline(pipeline,lava.writeRef()))) return false;
  pipeline.depthStencil.depthWriteEnable=false;targets[0].enableBlend=true;
  targets[0].color.srcFactor=targets[0].alpha.srcFactor=rhi::BlendFactor::SrcAlpha;
  targets[0].color.dstFactor=targets[0].alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  return SLANG_SUCCEEDED(device->createRenderPipeline(pipeline,transparent.writeRef()));
}
}
