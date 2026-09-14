#include "WorldRendererInternal.h"
#include "LocalShadowSystem.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
namespace {
constexpr float right[6][4]={{0,0,1,0},{0,0,-1,0},{1,0,0,0},{1,0,0,0},{-1,0,0,0},{1,0,0,0}};
constexpr float up[6][4]={{0,1,0,0},{0,1,0,0},{0,0,1,0},{0,0,-1,0},{0,1,0,0},{0,1,0,0}};
constexpr float forward[6][4]={{1,0,0,0},{-1,0,0,0},{0,1,0,0},{0,-1,0,0},{0,0,1,0},{0,0,-1,0}};
bool resources(WorldRenderer& r) {
  auto& s=r.local_shadows;const unsigned resolution=std::clamp(s.resolution,128u,1024u);
  if(s.allocated_resolution==resolution)return true;
  s.valid=false;
  for(unsigned face=0;face<6;++face) {
    rhi::TextureDesc d{};d.size={resolution,resolution,1};d.format=rhi::Format::D32Float;
    d.label="selected_local_shadow_face";d.defaultState=rhi::ResourceState::ShaderResource;
    d.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::ShaderResource;
    if(!world_rhi_ok(r.device->createTexture(d,nullptr,s.depth[face].writeRef())) ||
        !world_rhi_ok(s.depth[face]->getDefaultView(s.views[face].writeRef())))return false;
  }
  s.allocated_resolution=resolution;return true;
}
unsigned select_light(const WorldRenderer& r) {
  float best=0;unsigned selected=~0u;
  for(unsigned i=0;i<r.local_lighting.lights.size();++i) {
    const auto& light=r.local_lighting.lights[i];if(light.axis_v_type[3]==2 || light.position_range[3]<=.02f)continue;
    float distanceSquared=0;
    for(unsigned axis=0;axis<3;++axis) {float delta=light.position_range[axis]-r.draw_uniforms[axis];distanceSquared+=delta*delta;}
    const auto& color=light.color_intensity;
    float score=(.2126f*color[0]+.7152f*color[1]+.0722f*color[2])*color[3]/(1+distanceSquared);
    if(score>best) {best=score;selected=i;}
  }
  return selected;
}
bool draw_face(WorldRenderer& r,rhi::ICommandEncoder* commands,unsigned face) {
  auto& s=r.local_shadows;
  rhi::RenderPassDepthStencilAttachment depth{};depth.view=s.views[face];depth.depthClearValue=1;
  depth.depthLoadOp=rhi::LoadOp::Clear;depth.depthStoreOp=rhi::StoreOp::Store;
  depth.stencilLoadOp=rhi::LoadOp::DontCare;depth.stencilStoreOp=rhi::StoreOp::DontCare;
  rhi::RenderPassDesc desc{};desc.depthStencilAttachment=&depth;
  auto* pass=commands->beginRenderPass(desc);if(!pass)return false;
  rhi::RenderState state{};
  state.viewports[0]=rhi::Viewport::fromSize(float(s.allocated_resolution),float(s.allocated_resolution));state.viewportCount=1;
  state.scissorRects[0]=rhi::ScissorRect::fromSize(s.allocated_resolution,s.allocated_resolution);state.scissorRectCount=1;
  pass->setRenderState(state);auto* root=pass->bindPipeline(s.raster);bool ok=root && bind_world_atlas(r.atlas,root);
  if(ok) {
    rhi::ShaderCursor c(root);
    const unsigned voxel=r.local_lighting.lights[s.selected].axis_v_type[3]==3?1u:0u;
    ok=world_rhi_ok(c["localShadowVoxel"].setData(&voxel,sizeof(voxel)))&&world_rhi_ok(c["localShadowPosition"].setData(s.position.data(),16))&&world_rhi_ok(c["localShadowProjection"].setData(s.projection.data(),16))&&
      world_rhi_ok(c["localRight"].setData(right[face],16))&&world_rhi_ok(c["localUp"].setData(up[face],16))&&world_rhi_ok(c["localForward"].setData(forward[face],16));
    for(auto& [coordinate,column]:r.columns) {
      const unsigned opaque=column.pass_counts[0]+column.pass_counts[1],lava=column.pass_counts[4];
      if(!ok || (!opaque && !lava))continue;
      const float minimum[3]={float(coordinate.first)*32,float(column.min_y),float(coordinate.second)*32};
      const float maximum[3]={minimum[0]+32,minimum[1]+float(column.height),minimum[2]+32};
      float squared=0;
      for(unsigned axis=0;axis<3;++axis) {float distance=std::max({minimum[axis]-s.position[axis],0.f,s.position[axis]-maximum[axis]});squared+=distance*distance;}
      if(squared>s.position[3]*s.position[3])continue;
      const unsigned fluidBase=opaque+column.pass_counts[2];
      ok=world_rhi_ok(c["faces"].setBinding(column.faces))&&world_rhi_ok(c["fluidFaces"].setBinding(column.fluids))&&
        world_rhi_ok(c["fluidBase"].setData(&fluidBase,sizeof(fluidBase)));
      if(ok) {
        rhi::DrawArguments draw{};draw.vertexCount=6;
        if(opaque) {draw.instanceCount=opaque;pass->draw(draw);++s.draws;}
        // Glass and water transmit; lava retains its generated fluid corner heights.
        if(lava) {draw.instanceCount=lava;draw.startInstanceLocation=fluidBase+column.pass_counts[3];pass->draw(draw);++s.draws;}
      }
    }
  }
  if(ok)ok=render_player_shadow(r.player,pass,s.position.data(),right[face],up[face],forward[face],s.projection.data());
  pass->end();commands->setTextureState(s.depth[face],rhi::ResourceState::ShaderResource);return ok;
}
}
bool initialize_local_shadows(WorldRenderer& r) {
  const char* entries[]={"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/Shadows/LocalRaster.slang",entries,2,program))return false;
  rhi::RenderPipelineDesc p{};p.program=program;p.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  p.depthStencil.format=rhi::Format::D32Float;p.depthStencil.depthTestEnable=true;p.depthStencil.depthWriteEnable=true;
  p.depthStencil.depthFunc=rhi::ComparisonFunc::Less;p.rasterizer.cullMode=rhi::CullMode::None;
  return world_rhi_ok(r.device->createRenderPipeline(p,r.local_shadows.raster.writeRef()));
}
bool update_local_shadows(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.local_shadows;if(!resources(r))return false;
  const unsigned selected=select_light(r);
  if(selected==~0u) {s.selected=selected;s.valid=false;return true;}
  const auto& light=r.local_lighting.lights[selected];
  const float range=std::min(std::clamp(s.max_range,.021f,256.f),light.position_range[3]);
  const bool playerVisible=r.player && r.player_pose.visible;
  if(s.valid && s.selected==selected && s.scene_revision==r.scene_changes.revision() &&
      s.light_revision==r.local_lighting.light_revision && s.position[3]==range && !playerVisible && !s.player_visible) {
    s.projection[2]=std::clamp(s.origin_bias,.001f,.1f);return true;
  }
  s.valid=false;s.selected=selected;s.position=light.position_range;s.position[3]=range;
  const float nearPlane=.01f;
  s.projection={range/(range-nearPlane),range*nearPlane/(range-nearPlane),std::clamp(s.origin_bias,.001f,.1f),nearPlane};
  for(unsigned face=0;face<6;++face)if(!draw_face(r,commands,face))return false;
  s.scene_revision=r.scene_changes.revision();s.light_revision=r.local_lighting.light_revision;s.player_visible=playerVisible;
  ++s.map_updates;s.valid=true;return true;
}
bool bind_local_shadows(WorldRenderer& r,rhi::IShaderObject* root) {
  auto& s=r.local_shadows;rhi::ShaderCursor c(root);
  const char* names[]={"localShadow0","localShadow1","localShadow2","localShadow3","localShadow4","localShadow5"};
  for(unsigned face=0;face<6;++face)if(!world_rhi_ok(c[names[face]].setBinding(s.views[face])))return false;
  const unsigned info[4]={s.selected,s.valid?1u:0u,s.allocated_resolution,0};
  return world_rhi_ok(c["localShadowInfo"].setData(info,sizeof(info)))&&
    world_rhi_ok(c["localShadowPosition"].setData(s.position.data(),sizeof(s.position)))&&
    world_rhi_ok(c["localShadowProjection"].setData(s.projection.data(),sizeof(s.projection)));
}
}
