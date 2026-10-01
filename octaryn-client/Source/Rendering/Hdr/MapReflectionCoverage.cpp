#include "MapReflectionScreen.h"
#include "WorldRendererInternal.h"
#include "../../MapWorld/MapRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
bool prepare_map_reflection_coverage(WorldRenderer& r) {
  auto& q=r.map_reflections.queue;
  if(!q.screen_enabled || q.coverage)return true;
  rhi::ColorTargetDesc target{};target.format=rhi::Format::R32Uint;
  rhi::RenderPipelineDesc desc{};desc.targets=&target;desc.targetCount=1;
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.rasterizer.enableConservativeRasterization=true;
  desc.depthStencil.depthTestEnable=false;desc.depthStencil.depthWriteEnable=false;
  for(unsigned i=0;i<2;++i) {
    const char* entries[]={i?"coverage_item_vertex":"coverage_vertex","coverage_fragment"};
    Slang::ComPtr<rhi::IShaderProgram> program;
    if(!create_rhi_program(r.device,"octaryn-client/Shaders/Hdr/MapReflectionCoverage.slang",entries,2,program))return false;
    desc.program=program;auto& pipeline=i?q.item_coverage:q.coverage;
    if(SLANG_FAILED(r.device->createRenderPipeline(desc,pipeline.writeRef())))return false;
  }
  return true;
}
bool render_map_reflection_coverage(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& q=r.map_reflections.queue;if(!q.screen_enabled)return true;
  auto& hdr=r.target().hdr;
  if(!commands || hdr.attachment_count!=6 || !hdr.gbuffer[5] || !hdr.views[5] ||
      !q.coverage || !q.item_coverage) {
    r.status="screen_reflection_coverage_resources_missing";
    std::fprintf(stderr,"map_reflection_coverage_failed slot=%u attachments=%u texture=%u view=%u\n",
        r.active_frame,hdr.attachment_count,unsigned(bool(hdr.gbuffer[5])),unsigned(bool(hdr.views[5])));
    return false;
  }
  r.lighting_profile.begin_pass(commands,LightingPass::ReflectionCoverage);
  rhi::RenderPassColorAttachment color{};color.view=hdr.views[5];
  color.loadOp=rhi::LoadOp::Load;color.storeOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc desc{};desc.colorAttachments=&color;desc.colorAttachmentCount=1;
  auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  state.indexFormat=rhi::IndexFormat::Uint32;
  const auto draw=[&](MapRenderer& map,unsigned first,unsigned count,bool items) {
    if(map.lod_pixel_error>0 || map.lod_indices) {
      r.status="screen_reflections_require_native_geometry";return false;
    }
    state.indexBuffer={map.raster_indices.get(),0};pass->setRenderState(state);
    auto* root=pass->bindPipeline(items?q.item_coverage:q.coverage);if(!root)return false;
    rhi::ShaderCursor cursor(root);
    if(SLANG_FAILED(cursor["coverageVertices"].setBinding(rhi::Binding(map.vertices))) ||
        SLANG_FAILED(cursor["coverageView"].setData(r.view_uniforms.data(),5*16)))return false;
    if(items && SLANG_FAILED(cursor["coverageInstances"].setBinding(rhi::Binding(r.items.buffers[r.active_frame]))))return false;
    for(const auto& primitive:map.model.primitives) {
      if(!items && primitive.material.alpha_mode==MapAlphaMode::Opaque)continue;
      rhi::DrawArguments args{};args.vertexCount=primitive.index_count;args.startIndexLocation=primitive.first_index;
      args.instanceCount=count;args.startInstanceLocation=first;pass->drawIndexed(args);
    }
    return true;
  };
  bool ok=true;
  for(const auto& map:r.resident_maps)if(ok)ok=draw(*map,0,1,false);
  for(const auto& batch:r.items.batches)if(ok)ok=draw(*r.items.assets[batch.asset].mesh,batch.first,batch.count,true);
  pass->end();if(!ok)return false;
  commands->setTextureState(hdr.gbuffer[5],rhi::ResourceState::ShaderResource);
  r.lighting_profile.mark(commands,LightingPass::ReflectionCoverage);return true;
}
}
