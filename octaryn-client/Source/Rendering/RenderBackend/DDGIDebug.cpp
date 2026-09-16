#include "WorldRendererInternal.h"
#include "DDGIDebug.h"
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
bool world_ddgi_debug_initialize(WorldRenderer& r) {
  if(!world_ray_available(r))return true;
  const char* entries[]={"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/DDGI/ProbeDebug.slang",entries,2,program))return false;
  rhi::ColorTargetDesc target{};target.format=rhi::Format::RGBA16Float;
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=&target;desc.targetCount=1;
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;desc.rasterizer.cullMode=rhi::CullMode::None;
  if(!world_rhi_ok(r.device->createRenderPipeline(desc,r.ddgi.debug.writeRef())))return false;
  rhi::BufferDesc buffer{};buffer.size=16*32;buffer.elementSize=32;buffer.label="ddgi_dirty_boxes";
  buffer.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
  buffer.defaultState=rhi::ResourceState::ShaderResource;
  return world_rhi_ok(r.device->createBuffer(buffer,nullptr,r.ddgi.debug_box_buffer.writeRef()));
}
namespace {
bool world_ddgi_debug_boxes(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  if(!r.ray_debug.lines || !r.ddgi.debug_box_buffer)return true;
  struct Bounds {std::array<float,4> minimum,maximum;};
  std::vector<Bounds> boxes;
  const auto* volume=&r.ddgi;
  for(const auto& box:volume->debug_boxes) {
    if(!box.frame || box.frame+240<volume->frame)continue;
    boxes.push_back({{box.minimum[0],box.minimum[1],box.minimum[2],0},
      {box.maximum[0],box.maximum[1],box.maximum[2],0}});
  }
  if(volume->ignore_active)
    boxes.push_back({{float(volume->ignore_voxel[0]),float(volume->ignore_voxel[1]),float(volume->ignore_voxel[2]),0},
      {float(volume->ignore_voxel[0]+1),float(volume->ignore_voxel[1]+1),float(volume->ignore_voxel[2]+1),0}});
  if(boxes.empty() || boxes.size()>16)return true;
  commands->globalBarrier();
  if(!world_rhi_ok(commands->uploadBufferData(r.ddgi.debug_box_buffer,0,boxes.size()*sizeof(Bounds),boxes.data())))return false;
  rhi::RenderPassColorAttachment color{};color.view=r.target().hdr.scene_view;
  color.loadOp=rhi::LoadOp::Load;color.storeOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc desc{};desc.colorAttachments=&color;desc.colorAttachmentCount=1;
  auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(unsigned(r.render_width()),unsigned(r.render_height()));
  pass->setRenderState(state);auto* root=pass->bindPipeline(r.ray_debug.lines);
  const bool ok=root &&
    world_rhi_ok(rhi::ShaderCursor(root)["bounds"].setBinding(rhi::Binding(r.ddgi.debug_box_buffer.get()))) &&
    world_rhi_ok(rhi::ShaderCursor(root)["camera"].setData(r.draw_uniforms.data(),sizeof(float)*20));
  if(ok) {rhi::DrawArguments draw{};draw.vertexCount=24;draw.instanceCount=unsigned(boxes.size());pass->draw(draw);}
  pass->end();return ok;
}
}
bool world_ddgi_debug(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  const auto mode=r.lighting_settings.debug_view;
  if(mode<21 || mode>30 || !r.ray_enabled || !r.ddgi.available)return true;
  if(mode==27)return world_ddgi_debug_boxes(r,commands);
  if(mode>=28)return true;
  DDGISystem& s=r.ddgi;
  commands->globalBarrier();
  rhi::RenderPassColorAttachment color{};color.view=r.target().hdr.scene_view;
  color.loadOp=rhi::LoadOp::Load;color.storeOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc desc{};desc.colorAttachments=&color;desc.colorAttachmentCount=1;
  auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(unsigned(r.render_width()),unsigned(r.render_height()));
  pass->setRenderState(state);auto* root=pass->bindPipeline(r.ddgi.debug);
  const std::array<unsigned,4> grid{s.config.counts[0],s.config.counts[1],s.config.counts[2],1};
  const std::array<int,4> origin{s.origin[0],s.origin[1],s.origin[2],s.cell_centered?1:0};
  const std::array<float,4> parameters{s.config.spacing,s.config.hysteresis,s.config.max_distance,.2f};
  const std::array<unsigned,4> frame{static_cast<unsigned>(s.frame),s.config.rays,
    s.config.irradiance_resolution,s.config.visibility_resolution};
  const unsigned display=(mode==22 || mode==24)?1u:(mode>=25?2u:0u);
  rhi::ShaderCursor cursor(root);
  const bool ok=root &&
    world_rhi_ok(cursor["ddgiControls"].setBinding(rhi::Binding(s.controls.get()))) &&
    world_rhi_ok(cursor["ddgiProbes"].setBinding(rhi::Binding(s.probes.get()))) &&
    world_rhi_ok(cursor["ddgiIrradiance"].setBinding(rhi::Binding(s.irradiance.get()))) &&
    world_rhi_ok(cursor["ddgiVariability"].setBinding(rhi::Binding(s.variability.get()))) &&
    world_rhi_ok(cursor["ddgiGrid"].setData(&grid,sizeof(grid))) &&
    world_rhi_ok(cursor["ddgiOrigin"].setData(&origin,sizeof(origin))) &&
    world_rhi_ok(cursor["ddgiParameters"].setData(&parameters,sizeof(parameters))) &&
    world_rhi_ok(cursor["ddgiFrame"].setData(&frame,sizeof(frame))) &&
    world_rhi_ok(cursor["camera"].setData(r.draw_uniforms.data(),sizeof(float)*20)) &&
    world_rhi_ok(cursor["ddgiDebugMode"].setData(&display,sizeof(display)));
  if(ok) {rhi::DrawArguments draw{};draw.vertexCount=6;draw.instanceCount=s.stats.probe_count;pass->draw(draw);}
  pass->end();return ok;
}
}
