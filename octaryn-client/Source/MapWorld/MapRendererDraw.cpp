#include "MapRendererInternal.h"
#include "Camera.h"
#include "CameraMatrix.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace octaryn::client::rendering {
namespace {
// Same packing as the voxel draw list, including the jitter envelope.
::camera map_camera(const WorldCamera& eye,const WorldRenderer& r) {
  ::camera visibility{};
  camera_init(&visibility,CAMERA_PROJECTION_PERSPECTIVE);
  visibility.position[0]=eye.x;visibility.position[1]=eye.y;visibility.position[2]=eye.z;
  visibility.yaw_radians=eye.yaw;visibility.pitch_radians=eye.pitch;
  visibility.vertical_field_of_view_radians=eye.vertical_fov;visibility.far_plane=8192;
  camera_resize(&visibility,r.render_width(),r.render_height());camera_update(&visibility);
  if(eye.jitter_x!=0 || eye.jitter_y!=0) {
    visibility.projection[0][0]/=1+std::abs(eye.jitter_x);
    visibility.projection[1][1]/=1+std::abs(eye.jitter_y);
    float relative_view[4][4],relative_projection[4][4];
    std::memcpy(relative_view,visibility.view,sizeof(relative_view));
    relative_view[3][0]=relative_view[3][1]=relative_view[3][2]=0;
    camera_matrix_multiply(relative_projection,visibility.projection,relative_view);
    camera_matrix_extract_frustum(visibility.relative_frustum_planes,relative_projection);
  }
  return visibility;
}
struct MapDrawUniforms {
  float base_color[4];
  // Metallic, roughness, alpha cutoff.
  float material[4];
  float camera[4],right[4],up[4],forward[4],projection[4],light[4],settings[4];
  float sun[4],sky[4];
};
static_assert(sizeof(MapDrawUniforms)==11*16);
bool bind_shared(MapRenderer& map,rhi::IShaderObject* root) {
  rhi::ShaderCursor cursor(root);
  return SLANG_SUCCEEDED(cursor["mapVertices"].setBinding(rhi::Binding(map.vertices.get()))) &&
      (!cursor["mapIndices"].isValid() || SLANG_SUCCEEDED(cursor["mapIndices"].setBinding(rhi::Binding(map.indices.get())))) &&
      SLANG_SUCCEEDED(cursor["mapRayPrimitives"].setBinding(rhi::Binding(map.ray_primitives.get())));
}
bool draw_primitive(MapRenderer& map,rhi::IRenderPassEncoder* pass,rhi::IShaderObject* root,
    const MapPrimitive& primitive,const WorldRenderer& r) {
  MapDrawUniforms uniforms{};
  const auto& draw=r.draw_uniforms;
  for(size_t k=0;k<4;++k)uniforms.base_color[k]=primitive.material.base_color[k];
  uniforms.material[0]=primitive.material.metallic;
  uniforms.material[1]=primitive.material.roughness;
  uniforms.material[2]=primitive.material.alpha_cutoff;
  uniforms.material[3]=0;
  std::copy_n(draw.begin()+20,4,uniforms.light);
  uniforms.settings[0]=draw[29];
  uniforms.settings[1]=draw[25];
  uniforms.settings[2]=draw[34];
  uniforms.settings[3]=static_cast<float>(&primitive-map.model.primitives.data());
  for(size_t k=0;k<3;++k)uniforms.sun[k]=-r.sky.light_direction_sky[k];
  uniforms.sun[3]=r.lighting.sun_strength;
  uniforms.sky[0]=r.lighting.visual_sky_visibility;
  uniforms.sky[1]=r.lighting.ambient_strength;
  uniforms.sky[2]=r.sky.twilight_celestial_time[0];
  uniforms.sky[3]=r.fog_distance;
  std::copy_n(draw.begin(),4,uniforms.camera);
  std::copy_n(draw.begin()+4,4,uniforms.right);
  std::copy_n(draw.begin()+8,4,uniforms.up);
  std::copy_n(draw.begin()+12,4,uniforms.forward);
  std::copy_n(draw.begin()+16,4,uniforms.projection);
  rhi::ShaderCursor cursor(root);
  if(SLANG_FAILED(cursor["mapUniforms"].setData(&uniforms,sizeof(uniforms))))return false;
  rhi::DrawArguments draw_arguments{};
  draw_arguments.vertexCount=primitive.index_count;
  draw_arguments.startIndexLocation=primitive.first_index;
  pass->drawIndexed(draw_arguments);
  return true;
}
bool visible(const ::camera& visibility,const MapPrimitive& primitive) {
  return camera_is_box_visible(&visibility,primitive.bounds_min[0],primitive.bounds_min[1],
      primitive.bounds_min[2],primitive.bounds_max[0]-primitive.bounds_min[0],
      primitive.bounds_max[1]-primitive.bounds_min[1],
      primitive.bounds_max[2]-primitive.bounds_min[2])!=0;
}
void order_forward_blends(MapRenderer& map,const WorldCamera& eye) {
  std::vector<std::pair<float,std::uint32_t>> order;
  order.reserve(map.model.primitives.size());
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode!=MapAlphaMode::Blend)continue;
    const float dx=(primitive.bounds_min[0]+primitive.bounds_max[0])*.5f-eye.x,
        dy=(primitive.bounds_min[1]+primitive.bounds_max[1])*.5f-eye.y,
        dz=(primitive.bounds_min[2]+primitive.bounds_max[2])*.5f-eye.z;
    order.emplace_back(dx*dx+dy*dy+dz*dz,index);
  }
  // Preserve far-to-near blending like the transparent column pass.
  std::stable_sort(order.begin(),order.end(),
      [](const auto& a,const auto& b) {return a.first>b.first;});
  map.forward_order.resize(order.size());
  for(size_t index=0;index<order.size();++index)map.forward_order[index]=order[index].second;
}
}
bool render_map(MapRenderer* renderer,rhi::IRenderPassEncoder* pass,const WorldCamera& eye,
    WorldRenderer& r,bool forward) {
  if(!renderer || !pass)return false;
  auto& map=*renderer;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  // RHI tracks exclusive states: RT shaders read indices as SRV in this pass.
  // Keep a dedicated raster copy so that binding cannot invalidate IndexBuffer.
  state.indexBuffer={map.raster_indices.get(),0};state.indexFormat=rhi::IndexFormat::Uint32;
  pass->setRenderState(state);
  const auto visibility=map_camera(eye,r);
  const bool diagnostic=std::getenv("OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS")!=nullptr;
  const bool cull=std::getenv("OCTARYN_CLIENT_MAP_DISABLE_CULLING")==nullptr;
  unsigned submitted=0,culled=0;
  rhi::IShaderObject* root=nullptr;
  if(forward) {
    const bool ray=r.ray_effects && r.ray_enabled && world_ray_available(r) && map.forward_rt_pipeline;
    order_forward_blends(map,eye);
    for(const auto index:map.forward_order) {
      const auto& primitive=map.model.primitives[index];
      if(cull && !visible(visibility,primitive)) {++culled;continue;}
      if(!root) {
        root=pass->bindPipeline(ray?map.forward_rt_pipeline:map.forward_pipeline);
        if(!root || !bind_shared(map,root))return false;
        if(ray && (!bind_world_atlas(r.atlas,root) || !world_ray_bind(r,root) ||
            !bind_block_transport_lookup(r,root)))return false;
      }
      if(!draw_primitive(map,pass,root,primitive,r))return false;
      ++submitted;
    }
    if(diagnostic && r.frames%120==0)std::printf("map_draw forward=1 submitted=%u culled=%u\n",submitted,culled);
    return true;
  }
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    if(cull && !visible(visibility,primitive)) {++culled;continue;}
    if(!root) {
      root=pass->bindPipeline(map.gbuffer_pipeline);
      if(!root || !bind_shared(map,root))return false;
    }
    if(!draw_primitive(map,pass,root,primitive,r))return false;
    ++submitted;
  }
  if(diagnostic && r.frames%120==0)std::printf("map_draw forward=0 submitted=%u culled=%u\n",submitted,culled);
  return true;
}

bool render_map_shadow(MapRenderer* renderer,rhi::IRenderPassEncoder* pass,
    const std::array<float,4>& center,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward) {
  if(!renderer || !pass || !renderer->shadow_pipeline)return false;
  auto& map=*renderer;
  rhi::RenderState state{};state.indexBuffer={map.raster_indices.get(),0};
  state.indexFormat=rhi::IndexFormat::Uint32;pass->setRenderState(state);
  auto* root=pass->bindPipeline(map.shadow_pipeline);if(!root || !bind_shared(map,root))return false;
  rhi::ShaderCursor cursor(root);
  if(SLANG_FAILED(cursor["shadowCenter"].setData(center.data(),16)) ||
      SLANG_FAILED(cursor["shadowRight"].setData(right.data(),16)) ||
      SLANG_FAILED(cursor["shadowUp"].setData(up.data(),16)) ||
      SLANG_FAILED(cursor["shadowForward"].setData(forward.data(),16)))return false;
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    const float radius=std::sqrt(
        (primitive.bounds_max[0]-primitive.bounds_min[0])*(primitive.bounds_max[0]-primitive.bounds_min[0])+
        (primitive.bounds_max[1]-primitive.bounds_min[1])*(primitive.bounds_max[1]-primitive.bounds_min[1])+
        (primitive.bounds_max[2]-primitive.bounds_min[2])*(primitive.bounds_max[2]-primitive.bounds_min[2]))*.5f;
    const float px=(primitive.bounds_min[0]+primitive.bounds_max[0])*.5f-center[0];
    const float py=(primitive.bounds_min[1]+primitive.bounds_max[1])*.5f-center[1];
    const float pz=(primitive.bounds_min[2]+primitive.bounds_max[2])*.5f-center[2];
    const auto projected=[&](const std::array<float,4>& axis) {return px*axis[0]+py*axis[1]+pz*axis[2];};
    if(std::abs(projected(right))>center[3]+radius || std::abs(projected(up))>center[3]+radius ||
        projected(forward)>1024.f+radius || projected(forward)<-radius)continue;
    if(SLANG_FAILED(cursor["shadowMaterial"].setData(&index,sizeof(index))))return false;
    rhi::DrawArguments draw{};draw.vertexCount=primitive.index_count;
    draw.startIndexLocation=primitive.first_index;pass->drawIndexed(draw);
  }
  return true;
}

bool render_map_local_shadow(MapRenderer* renderer,rhi::IRenderPassEncoder* pass,
    const std::array<float,4>& position,const std::array<float,4>& projection,
    const std::array<float,4>& right,const std::array<float,4>& up,
    const std::array<float,4>& forward) {
  if(!renderer || !pass || !renderer->local_shadow_pipeline)return false;
  auto& map=*renderer;
  rhi::RenderState state{};state.indexBuffer={map.raster_indices.get(),0};
  state.indexFormat=rhi::IndexFormat::Uint32;pass->setRenderState(state);
  auto* root=pass->bindPipeline(map.local_shadow_pipeline);if(!root || !bind_shared(map,root))return false;
  rhi::ShaderCursor cursor(root);
  if(SLANG_FAILED(cursor["localShadowPosition"].setData(position.data(),16)) ||
      SLANG_FAILED(cursor["localShadowProjection"].setData(projection.data(),16)) ||
      SLANG_FAILED(cursor["localRight"].setData(right.data(),16)) ||
      SLANG_FAILED(cursor["localUp"].setData(up.data(),16)) ||
      SLANG_FAILED(cursor["localForward"].setData(forward.data(),16)))return false;
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    float squared=0;
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=std::max({primitive.bounds_min[axis]-position[axis],0.f,
          position[axis]-primitive.bounds_max[axis]});squared+=delta*delta;
    }
    if(squared>position[3]*position[3])continue;
    if(SLANG_FAILED(cursor["localShadowMaterial"].setData(&index,sizeof(index))))return false;
    rhi::DrawArguments draw{};draw.vertexCount=primitive.index_count;
    draw.startIndexLocation=primitive.first_index;pass->drawIndexed(draw);
  }
  return true;
}
}
