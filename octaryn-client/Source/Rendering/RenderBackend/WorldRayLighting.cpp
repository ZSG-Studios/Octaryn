#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>

namespace octaryn::client::rendering {
bool world_ray_lighting_initialize(WorldRenderer& r) {
  if(!world_ray_available(r))return true;
  const char* entries[]={"forward_vertex_main","forward_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/RayTracing/WorldRayRaster.slang",entries,2,program))return false;
  rhi::ColorTargetDesc target{};target.format=rhi::Format::RGBA16Float;target.enableBlend=true;
  target.color.srcFactor=target.alpha.srcFactor=rhi::BlendFactor::SrcAlpha;
  target.color.dstFactor=target.alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  rhi::RenderPipelineDesc pipeline{};pipeline.program=program;pipeline.targets=&target;pipeline.targetCount=1;
  pipeline.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  pipeline.depthStencil.format=rhi::Format::D32Float;pipeline.depthStencil.depthTestEnable=true;
  pipeline.depthStencil.depthWriteEnable=false;pipeline.depthStencil.depthFunc=rhi::ComparisonFunc::Less;
  pipeline.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;pipeline.rasterizer.cullMode=rhi::CullMode::None;
  return world_rhi_ok(r.device->createRenderPipeline(pipeline,r.ray_water_pipeline.writeRef()));
}
}
