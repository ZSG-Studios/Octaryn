#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
bool initialize_shadow_fallback(WorldRenderer& r) {
  const char* entries[]={"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/Shadows/ClipmapRaster.slang",entries,2,program))return false;
  rhi::RenderPipelineDesc p{};p.program=program;p.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  p.depthStencil.format=rhi::Format::D32Float;p.depthStencil.depthTestEnable=true;p.depthStencil.depthWriteEnable=true;
  p.depthStencil.depthFunc=rhi::ComparisonFunc::Less;p.rasterizer.cullMode=rhi::CullMode::None;
  return world_rhi_ok(r.device->createRenderPipeline(p,r.shadow_fallback.raster.writeRef())) &&
    create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Shadows/ClipmapResolve.slang","main",r.shadow_fallback.resolve);
}
bool update_shadow_fallback(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.shadow_fallback;auto& hdr=r.target().hdr;
  const unsigned resolution=std::clamp(r.lighting_settings.shadow_resolution,256u,2048u);
  if(s.resolution!=resolution) {
    // Quality changes use frame-drained reconfiguration in the public setter.
    s.resolution=resolution;
    for(unsigned i=0;i<3;++i) {
      s.views[i].setNull();s.depth[i].setNull();rhi::TextureDesc d{};
      d.size={resolution,resolution,1};d.format=rhi::Format::D32Float;d.label="sun_shadow_clipmap";
      d.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::ShaderResource;d.defaultState=rhi::ResourceState::ShaderResource;
      if(!world_rhi_ok(r.device->createTexture(d,nullptr,s.depth[i].writeRef())) ||
         !world_rhi_ok(s.depth[i]->getDefaultView(s.views[i].writeRef())))return false;
    }
  }
  std::array<float,4> forward={r.sky.light_direction_sky[0],r.sky.light_direction_sky[1],r.sky.light_direction_sky[2],0};
  auto cross=[](const auto& a,const auto& b) {return std::array<float,4>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0],0};};
  auto normalize=[](auto& a) {const float length=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);for(unsigned i=0;i<3;++i)a[i]/=std::max(length,.001f);};
  normalize(forward);const std::array<float,4> axis=std::abs(forward[1])<.95f?std::array<float,4>{0,1,0,0}:std::array<float,4>{1,0,0,0};
  auto right=cross(axis,forward);normalize(right);auto up=cross(forward,right);
  const float eye[4]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2],0};
  auto dot=[&](const auto& a) {return eye[0]*a[0]+eye[1]*a[1]+eye[2]*a[2];};
  std::array<std::array<float,4>,3> centers{};
  const float spans[3]={64,256,1024};
  for(unsigned level=0;level<3;++level) {
    const float texel=spans[level]*2/float(resolution);
    const float x=std::floor(dot(right)/texel)*texel,y=std::floor(dot(up)/texel)*texel,z=dot(forward);
    for(unsigned k=0;k<3;++k)centers[level][k]=right[k]*x+up[k]*y+forward[k]*z;
    centers[level][3]=spans[level];
    rhi::RenderPassDepthStencilAttachment attachment{};attachment.view=s.views[level];attachment.depthClearValue=1;
    attachment.depthLoadOp=rhi::LoadOp::Clear;attachment.depthStoreOp=rhi::StoreOp::Store;
    rhi::RenderPassDesc desc{};desc.depthStencilAttachment=&attachment;
    auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
    rhi::RenderState state{};state.viewports[0]=rhi::Viewport::fromSize(float(resolution),float(resolution));state.viewportCount=1;
    state.scissorRects[0]=rhi::ScissorRect::fromSize(resolution,resolution);state.scissorRectCount=1;pass->setRenderState(state);
    auto* root=pass->bindPipeline(s.raster);bool ok=root && bind_world_atlas(r.atlas,root);
    if(ok) {
      rhi::ShaderCursor c(root);
      ok=world_rhi_ok(c["center"].setData(centers[level].data(),16)) && world_rhi_ok(c["lightRight"].setData(right.data(),16)) &&
        world_rhi_ok(c["lightUp"].setData(up.data(),16)) && world_rhi_ok(c["lightForward"].setData(forward.data(),16));
      for(auto& [coordinate,column]:r.columns) {
        const auto count=column.pass_counts[0]+column.pass_counts[1];
        const auto lava=column.pass_counts[4];if((!count && !lava) || !ok)continue;
        // Include off-camera casters; reject columns outside this light-space clipmap.
        float p[3]={float(coordinate.first)*32+16-centers[level][0],float(column.min_y)+float(column.height)*.5f-centers[level][1],float(coordinate.second)*32+16-centers[level][2]};
        const float radius=std::sqrt(512.f+float(column.height)*float(column.height)*.25f);
        auto distance=[&](const auto& a) {return std::abs(p[0]*a[0]+p[1]*a[1]+p[2]*a[2]);};
        if(distance(right)>spans[level]+radius || distance(up)>spans[level]+radius || distance(forward)>1024+radius)continue;
        const unsigned fluid_base=count+column.pass_counts[2];
        ok=world_rhi_ok(c["faces"].setBinding(column.faces)) && world_rhi_ok(c["fluidFaces"].setBinding(column.fluids)) &&
          world_rhi_ok(c["fluidBase"].setData(&fluid_base,sizeof(fluid_base)));
        if(ok) {
          rhi::DrawArguments draw{};draw.vertexCount=6;
          if(count) {draw.instanceCount=count;pass->draw(draw);}
          // Glass/water transmit sunlight. Lava uses its actual retained fluid mesh.
          if(lava) {draw.instanceCount=lava;draw.startInstanceLocation=fluid_base+column.pass_counts[3];pass->draw(draw);}
        }
      }
    }
    pass->end();if(!ok)return false;commands->setTextureState(s.depth[level],rhi::ResourceState::ShaderResource);
  }
  auto* pass=commands->beginComputePass();if(!pass)return false;auto* root=pass->bindPipeline(s.resolve);bool ok=root!=nullptr;
  const unsigned extent[2]={unsigned(r.render_width()),unsigned(r.render_height())};
  if(ok) {
    rhi::ShaderCursor c(root);
    ok=world_rhi_ok(c["level0"].setBinding(s.views[0])) && world_rhi_ok(c["level1"].setBinding(s.views[1])) && world_rhi_ok(c["level2"].setBinding(s.views[2])) &&
      world_rhi_ok(c["positions"].setBinding(hdr.views[1])) && world_rhi_ok(c["voxels"].setBinding(hdr.views[2])) &&
      world_rhi_ok(c["visibility"].setBinding(hdr.sun_visibility_view)) && world_rhi_ok(c["eye"].setData(eye,sizeof(eye))) &&
      world_rhi_ok(c["centers"].setData(centers.data(),sizeof(centers))) && world_rhi_ok(c["lightRight"].setData(right.data(),16)) &&
      world_rhi_ok(c["lightUp"].setData(up.data(),16)) && world_rhi_ok(c["lightForward"].setData(forward.data(),16)) && world_rhi_ok(c["extent"].setData(extent,sizeof(extent)));
  }
  if(ok)pass->dispatchCompute((extent[0]+7)/8,(extent[1]+7)/8,1);pass->end();hdr.ray_shadows=ok;
  commands->setTextureState(hdr.sun_visibility,rhi::ResourceState::ShaderResource);return ok;
}
}
