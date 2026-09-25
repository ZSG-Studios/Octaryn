#include "WorldRayDebug.h"
#include "WorldRayTracingState.h"
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
namespace {
bool bounds_overlay(WorldRenderer&,rhi::ICommandEncoder*) {
  // Column BLAS bounds diagnostic; returns with the voxel world's geometry feeder.
  return true;
}
bool world_ray_debug_initialize(WorldRenderer& r) {
  if(!world_ray_available(r))return true;
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/RayTracing/Debug.slang","main",r.ray_debug.trace))return false;
  const char* entries[]={"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/RayTracing/BoundsDebug.slang",entries,2,program))return false;
  rhi::ColorTargetDesc target{};target.format=rhi::Format::RGBA16Float;
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=&target;desc.targetCount=1;
  desc.primitiveTopology=rhi::PrimitiveTopology::LineList;desc.rasterizer.cullMode=rhi::CullMode::None;
  return world_rhi_ok(r.device->createRenderPipeline(desc,r.ray_debug.lines.writeRef()));
}
bool world_ray_debug(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  const auto mode=r.lighting_settings.debug_view;
  if(mode<9 || mode>11 || !r.ray_enabled || !world_ray_available(r))return true;
  commands->globalBarrier();
  if(mode==10)return bounds_overlay(r,commands);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(r.ray_debug.trace);
  bool ok=root && world_ray_bind(r,root) && bind_world_atlas(r.atlas,root);
  if(ok) {
    rhi::ShaderCursor c(root);const unsigned extent[2]={unsigned(r.render_width()),unsigned(r.render_height())};
    ok=world_rhi_ok(c["positions"].setBinding(r.target().hdr.views[1])) &&
      world_rhi_ok(c["scene"].setBinding(r.target().hdr.scene_view)) &&
      world_rhi_ok(c["eye"].setData(r.view_uniforms.data(),sizeof(float)*4)) &&
      world_rhi_ok(c["extent"].setData(extent,sizeof(extent))) && world_rhi_ok(c["debugMode"].setData(&mode,sizeof(mode)));
  }
  if(ok)pass->dispatchCompute(unsigned(r.render_width()+7)/8,unsigned(r.render_height()+7)/8,1);
  pass->end();
  if(ok)commands->setTextureState(r.target().hdr.scene,rhi::ResourceState::ShaderResource);
  return ok;
}
}
