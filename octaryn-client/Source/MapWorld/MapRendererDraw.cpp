#include "MapRendererInternal.h"
#include "MapDrawBinding.h"
#include "Camera.h"
#include "MapVisibility.h"
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
bool draw_primitive(MapRenderer& map,rhi::IRenderPassEncoder* pass,rhi::IShaderObject* root,
    const MapPrimitive& primitive,const WorldRenderer& r) {
  if(!bind_map_draw_uniforms(map,root,primitive,r))return false;
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
void append_forward(MapRenderer& map,const WorldCamera& eye,const ::camera& visibility,
    bool cull,WorldRenderer& r,unsigned& culled) {
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode!=MapAlphaMode::Blend)continue;
    if(cull && !visible(visibility,primitive)) {++culled;continue;}
    const float dx=(primitive.bounds_min[0]+primitive.bounds_max[0])*.5f-eye.x,
        dy=(primitive.bounds_min[1]+primitive.bounds_max[1])*.5f-eye.y,
        dz=(primitive.bounds_min[2]+primitive.bounds_max[2])*.5f-eye.z;
    r.map_forward_order.push_back({&map,index,dx*dx+dy*dy+dz*dz,static_cast<std::uint32_t>(r.map_forward_order.size())});
  }
}
bool draw_forward(rhi::IRenderPassEncoder* pass,WorldRenderer& r,unsigned culled) {
  auto& order=r.map_forward_order;
  std::sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.distance==b.distance?a.order<b.order:a.distance>b.distance;});
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  state.indexFormat=rhi::IndexFormat::Uint32;
  MapRenderer* previous=nullptr;rhi::IShaderObject* root=nullptr;
  const bool ray_enabled=r.ray_effects && r.ray_enabled && world_ray_available(r);
  for(const auto& draw:order) {
    auto& map=*draw.map;
    if(previous!=&map) {
      state.indexBuffer={map.raster_indices.get(),0};pass->setRenderState(state);
      const bool ray=ray_enabled && map.forward_rt_pipeline;
      root=pass->bindPipeline(ray?map.forward_rt_pipeline:map.forward_pipeline);
      if(!root || !bind_map_geometry(map,root))return false;
      if(ray && (!bind_world_atlas(r.atlas,root) || !world_ray_bind(r,root) || !bind_block_transport_lookup(r,root)))return false;
      previous=&map;
    }
    if(!draw_primitive(map,pass,root,map.model.primitives[draw.primitive],r))return false;
  }
  if(std::getenv("OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS") && r.frames%120==0)
    std::printf("map_draw forward=1 global_sort=1 submitted=%zu culled=%u\n",order.size(),culled);
  return true;
}
}
bool render_map(MapRenderer* renderer,rhi::IRenderPassEncoder* pass,const WorldCamera& eye,
    WorldRenderer& r,bool forward,bool phase2) {
  if(!renderer || !pass)return false;
  auto& map=*renderer;
  // Phase 2 draws only the occlusion-retested set of fused-table indirect maps.
  if(phase2 && (!map.indirect_enabled || map.cull_slot<0))return true;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  // RHI tracks exclusive states: RT shaders read indices as SRV in this pass.
  // Keep a dedicated raster copy so that binding cannot invalidate IndexBuffer.
  state.indexBuffer={!forward && map.lod_indices?map.lod_indices.get():map.raster_indices.get(),0};state.indexFormat=rhi::IndexFormat::Uint32;
  pass->setRenderState(state);
  const auto visibility=map_visibility_camera(eye,r);
  const bool diagnostic=std::getenv("OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS")!=nullptr;
  const bool cull=std::getenv("OCTARYN_CLIENT_MAP_DISABLE_CULLING")==nullptr;
  unsigned submitted=0,culled=0;
  rhi::IShaderObject* root=nullptr;
  if(forward) {
    r.map_forward_order.clear();append_forward(map,eye,visibility,cull,r,culled);
    return draw_forward(pass,r,culled);
  }
  if(map.meshlet_enabled) {
    if(!map.meshlet_count)return true;
    state.indexBuffer={};pass->setRenderState(state);
    root=pass->bindPipeline(map.meshlet_pipeline);
    if(!root || !bind_map_geometry(map,root) || !bind_map_draw_uniforms(map,root,map.model.primitives.front(),r))return false;
    const unsigned width=std::min(map.meshlet_count,65535u);
    const char* cone_env=std::getenv("OCTARYN_CLIENT_MAP_CONE_CULLING");
    const unsigned cone=!cone_env||std::strcmp(cone_env,"0")!=0?1u:0u;
    const unsigned settings[4]={map.meshlet_count,width,cull?1u:0u,cone};
    rhi::ShaderCursor cursor(root);
    if(SLANG_FAILED(cursor["mapMeshlets"].setBinding(rhi::Binding(map.meshlets))) ||
        SLANG_FAILED(cursor["mapMeshletVertices"].setBinding(rhi::Binding(map.meshlet_vertices))) ||
        SLANG_FAILED(cursor["mapMeshletTriangles"].setBinding(rhi::Binding(map.meshlet_triangles))) ||
        SLANG_FAILED(cursor["mapMeshletPlanes"].setData(visibility.relative_frustum_planes,sizeof(visibility.relative_frustum_planes))) ||
        SLANG_FAILED(cursor["mapMeshletSettings"].setData(settings,sizeof(settings))))return false;
    pass->drawMeshTasks(width,(map.meshlet_count+width-1)/width,1);
    if(diagnostic && r.frames%120==0)std::printf("map_draw forward=0 meshlet=1 meshlets=%u cpu_submissions=1\n",map.meshlet_count);
    return true;
  }
  if(map.indirect_enabled) {
    // Without a fused-table slot the direct per-primitive loop below is the fallback.
    if(map.cull_slot>=0) {
      root=pass->bindPipeline(map.indirect_gbuffer_pipeline);
      if(!root || !bind_map_geometry(map,root) || !bind_map_draw_uniforms(map,root,map.model.primitives.front(),r))return false;
      const unsigned count=static_cast<unsigned>(map.model.primitives.size());
      const auto offset=static_cast<std::uint64_t>(map.cull_slot)*MapCullSet::kSlotArguments*4;
      pass->drawIndexedIndirect(count,{(phase2?r.map_cull.arguments_b:r.map_cull.arguments_a).get(),offset});
      if(diagnostic && r.frames%120==0) {
        if(phase2)std::printf("map_draw forward=0 indirect_phase2=1 command_slots=%u cpu_submissions=1\n",count);
        else std::printf("map_draw forward=0 indirect=1 command_slots=%u cpu_submissions=1\n",count);
      }
      return true;
    }
  }
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    if(cull && !visible(visibility,primitive)) {++culled;continue;}
    if(!root) {
      root=pass->bindPipeline(map.gbuffer_pipeline);
      if(!root || !bind_map_geometry(map,root))return false;
    }
    if(!draw_primitive(map,pass,root,primitive,r))return false;
    ++submitted;
  }
  if(diagnostic && r.frames%120==0)std::printf("map_draw forward=0 submitted=%u culled=%u\n",submitted,culled);
  return true;
}

bool render_maps_forward(std::span<const std::shared_ptr<MapRenderer>> maps,rhi::IRenderPassEncoder* pass,
    const WorldCamera& eye,WorldRenderer& r) {
  if(!pass)return false;
  r.map_forward_order.clear();unsigned culled=0;
  const auto visibility=map_visibility_camera(eye,r);
  const bool cull=std::getenv("OCTARYN_CLIENT_MAP_DISABLE_CULLING")==nullptr;
  for(const auto& map:maps)if(map)append_forward(*map,eye,visibility,cull,r,culled);
  return draw_forward(pass,r,culled);
}

bool render_map_shadow(MapRenderer* renderer,rhi::IRenderPassEncoder* pass,
    const std::array<float,4>& center,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward) {
  if(!renderer || !pass || !renderer->shadow_pipeline)return false;
  auto& map=*renderer;
  rhi::RenderState state{};state.indexBuffer={map.raster_indices.get(),0};
  state.indexFormat=rhi::IndexFormat::Uint32;pass->setRenderState(state);
  auto* root=pass->bindPipeline(map.shadow_pipeline);if(!root || !bind_map_geometry(map,root))return false;
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
  auto* root=pass->bindPipeline(map.local_shadow_pipeline);if(!root || !bind_map_geometry(map,root))return false;
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
