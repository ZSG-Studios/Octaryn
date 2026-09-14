#include "WorldRayDebug.h"
#include "WorldRayTracingState.h"
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
namespace {
bool bounds_overlay(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  const auto& scene=r.ray_tracing->state->frames[r.active_frame].snapshot;
  if(!scene || scene->columns.empty())return true;
  auto& slot=r.ray_debug.frames[r.active_frame];
  if(!slot.bounds || slot.generation!=scene->generation) {
    struct Bounds {std::array<float,4> minimum,maximum;};
    std::vector<Bounds> boxes;boxes.reserve(scene->columns.size());
    for(const auto& column:scene->columns) {
      const auto x=static_cast<std::int32_t>(column->record.reserved[0]);
      const auto z=static_cast<std::int32_t>(column->record.reserved[1]);
      const auto found=r.columns.find({x,z});
      if(found==r.columns.end())return false;
      const float width=float(world_presentation::StreamColumnWidth);
      boxes.push_back({{float(x)*width,float(found->second.min_y),float(z)*width,0},
        {float(x)*width+width,float(found->second.min_y+found->second.height),float(z)*width+width,0}});
    }
    const auto size=boxes.size()*sizeof(Bounds);
    if(!world_ray::buffer(r,size,sizeof(Bounds),rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,
      rhi::ResourceState::ShaderResource,slot.bounds) ||
      !world_rhi_ok(commands->uploadBufferData(slot.bounds,0,size,boxes.data())))return false;
    slot.count=static_cast<std::uint32_t>(boxes.size());slot.generation=scene->generation;
  }
  rhi::RenderPassColorAttachment color{};color.view=r.target().hdr.scene_view;
  color.loadOp=rhi::LoadOp::Load;color.storeOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc desc{};desc.colorAttachments=&color;desc.colorAttachmentCount=1;
  auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(unsigned(r.render_width()),unsigned(r.render_height()));
  pass->setRenderState(state);auto* root=pass->bindPipeline(r.ray_debug.lines);
  const bool ok=root && world_rhi_ok(rhi::ShaderCursor(root)["bounds"].setBinding(rhi::Binding(slot.bounds))) &&
    world_rhi_ok(rhi::ShaderCursor(root)["camera"].setData(r.draw_uniforms.data(),sizeof(float)*20));
  if(ok) {rhi::DrawArguments draw{};draw.vertexCount=24;draw.instanceCount=slot.count;pass->draw(draw);}
  pass->end();return ok;
}
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
      world_rhi_ok(c["eye"].setData(r.draw_uniforms.data(),sizeof(float)*4)) &&
      world_rhi_ok(c["extent"].setData(extent,sizeof(extent))) && world_rhi_ok(c["debugMode"].setData(&mode,sizeof(mode)));
  }
  if(ok)pass->dispatchCompute(unsigned(r.render_width()+7)/8,unsigned(r.render_height()+7)/8,1);
  pass->end();
  if(ok)commands->setTextureState(r.target().hdr.scene,rhi::ResourceState::ShaderResource);
  return ok;
}
}
