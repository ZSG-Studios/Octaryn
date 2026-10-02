#include "MapTransformDiagnostics.h"
#include "MapRendererInternal.h"
#include "MapDrawBinding.h"
#include "Camera.h"
#include "MapVisibility.h"
#include "WorldRendererInternal.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace octaryn::client::rendering {
namespace {
bool draw_primitive(MapRenderer& map,rhi::IRenderPassEncoder* pass,rhi::IShaderObject* root,
    const MapPrimitive& primitive,const WorldRenderer& r,const virtual_geometry::GeometryTransform* instance=nullptr) {
  if(!bind_map_draw_uniforms(map,root,primitive,r,instance))return false;
  rhi::DrawArguments draw_arguments{};
  draw_arguments.vertexCount=primitive.index_count;
  const auto index=std::size_t(&primitive-map.model.primitives.data());
  draw_arguments.startIndexLocation=map.forward_first_indices.empty()?primitive.first_index:map.forward_first_indices.at(index);
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
    for(std::size_t instance=0;instance<std::max<std::size_t>(1,map.geometry_instances.size());++instance) {
      auto draw=primitive;
      if(!map.geometry_instances.empty()) {
        const auto bounds=virtual_geometry::geometry_transform_bounds(map.geometry_instances[instance],
            {primitive.bounds_min[0],primitive.bounds_min[1],primitive.bounds_min[2],primitive.bounds_max[0],primitive.bounds_max[1],primitive.bounds_max[2]});
        std::copy_n(bounds.begin(),3,draw.bounds_min);std::copy_n(bounds.begin()+3,3,draw.bounds_max);
      }
      if(cull && !visible(visibility,draw)) {++culled;continue;}
      const float dx=(draw.bounds_min[0]+draw.bounds_max[0])*.5f-eye.x,
          dy=(draw.bounds_min[1]+draw.bounds_max[1])*.5f-eye.y,
          dz=(draw.bounds_min[2]+draw.bounds_max[2])*.5f-eye.z;
      r.map_forward_order.push_back({&map,index,dx*dx+dy*dy+dz*dz,static_cast<std::uint32_t>(r.map_forward_order.size()),
          map.geometry_instances.empty()?UINT32_MAX:static_cast<std::uint32_t>(instance)});
    }
  }
}
bool draw_forward(rhi::IRenderPassEncoder* pass,WorldRenderer& r,unsigned culled) {
  auto& order=r.map_forward_order;
  std::sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.distance==b.distance?a.order<b.order:a.distance>b.distance;});
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  state.indexFormat=rhi::IndexFormat::Uint32;
  MapRenderer* previous=nullptr;bool previous_additive=false;rhi::IShaderObject* root=nullptr;
  const bool ray_enabled=r.ray_effects && r.ray_enabled && world_ray_available(r);
  for(const auto& draw:order) {
    auto& map=*draw.map;
    const bool additive=map.model.primitives[draw.primitive].material.additive;
    if(previous!=&map || previous_additive!=additive) {
      state.indexBuffer={map.raster_indices.get(),0};pass->setRenderState(state);
      const bool ray=ray_enabled && map.forward_rt_pipeline;
      root=pass->bindPipeline(additive?(ray?map.additive_rt_pipeline:map.additive_pipeline):(ray?map.forward_rt_pipeline:map.forward_pipeline));
      if(!root || !bind_map_geometry(map,root))return false;
      if(ray && (!bind_world_atlas(r.atlas,root) || !world_ray_bind(r,root) || !bind_block_transport_lookup(r,root)))return false;
      previous=&map;previous_additive=additive;
    }
    const auto* instance=draw.instance==UINT32_MAX?nullptr:&map.geometry_instances[draw.instance];
    if(!draw_primitive(map,pass,root,map.model.primitives[draw.primitive],r,instance))return false;
  }
  if(std::getenv("OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS") && r.frames%120==0)
    std::printf("map_draw forward=1 global_sort=1 submitted=%zu culled=%u\n",order.size(),culled);
  return true;
}
}
bool render_maps_forward(std::span<const std::shared_ptr<MapRenderer>> maps,rhi::IRenderPassEncoder* pass,
    const WorldCamera& eye,WorldRenderer& r) {
  if(!pass)return false;
  r.map_forward_order.clear();unsigned culled=0;
  const auto visibility=map_visibility_camera(eye,r);
  const bool cull=std::getenv("OCTARYN_CLIENT_MAP_DISABLE_CULLING")==nullptr;
  for(const auto& map:maps)if(map) {if(!map_transform_diagnostics_frame(*map,r.frames,eye,cull,0,0,0,"sorted_forward"))return false;append_forward(*map,eye,visibility,cull,r,culled);}
  return draw_forward(pass,r,culled);
}
}
