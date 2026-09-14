#include "WorldRendererInternal.h"
#include "Camera.h"
#include "CameraMatrix.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
namespace octaryn::client::rendering {
void world_renderer_prepare_draw(WorldRenderer& r,const WorldCamera& eye) {
  const float sy=std::sin(eye.yaw),cy=std::cos(eye.yaw),sp=std::sin(eye.pitch),cp=std::cos(eye.pitch);
  const float focal=1/std::tan(std::clamp(eye.vertical_fov,.2f,2.7f)/2);
  constexpr float near_plane=.1f,far_plane=8192;
  r.draw_uniforms={eye.x,eye.y,eye.z,0,cy,0,sy,eye.jitter_x,-sy*sp,cp,cy*sp,eye.jitter_y,sy*cp,sp,-cy*cp,0,
    focal*static_cast<float>(r.render_height())/static_cast<float>(r.render_width()),focal,far_plane/(far_plane-near_plane),near_plane*far_plane/(far_plane-near_plane),
    r.sky.light_direction_sky[0],r.sky.light_direction_sky[1],r.sky.light_direction_sky[2],r.lighting.visual_sky_visibility,
    r.sky.twilight_celestial_time[3],r.sky.twilight_celestial_time[0],r.sky.twilight_celestial_time[1],r.lighting.sun_strength,
    0,r.fog_distance,r.temporal.mode?std::log2(float(r.render_width())/float(r.width))-1.f:0,0,
    static_cast<float>(r.pbr),static_cast<float>(r.pom),r.lighting.skylight_floor,r.lighting.ambient_strength};
  world_prepare_draw_list(r.draw_list,r.columns,eye,r.render_width(),r.render_height(),r.culling_enabled);
  r.drawn_columns=static_cast<std::uint32_t>(r.draw_list.visible.size());
  r.drawn_quads=r.draw_list.quads;
}
void world_prepare_draw_list(WorldDrawList& list,
    std::map<std::pair<std::int32_t,std::int32_t>,WorldColumnGpu>& columns,
    const WorldCamera& eye,int width,int height,bool culling_enabled) {
  ::camera visibility{};
  camera_init(&visibility,CAMERA_PROJECTION_PERSPECTIVE);
  visibility.position[0]=eye.x;visibility.position[1]=eye.y;visibility.position[2]=eye.z;
  visibility.yaw_radians=eye.yaw;visibility.pitch_radians=eye.pitch;
  visibility.vertical_field_of_view_radians=eye.vertical_fov;visibility.far_plane=8192;
  camera_resize(&visibility,width,height);camera_update(&visibility);
  if(eye.jitter_x!=0 || eye.jitter_y!=0) {
    // A symmetric envelope contains both the unjittered and shifted raster
    // frustum. Keep near/far unchanged and preserve the exact Off path.
    visibility.projection[0][0]/=1+std::abs(eye.jitter_x);
    visibility.projection[1][1]/=1+std::abs(eye.jitter_y);
    float relative_view[4][4],relative_projection[4][4];
    std::memcpy(relative_view,visibility.view,sizeof(relative_view));
    relative_view[3][0]=relative_view[3][1]=relative_view[3][2]=0;
    camera_matrix_multiply(relative_projection,visibility.projection,relative_view);
    camera_matrix_extract_frustum(visibility.relative_frustum_planes,relative_projection);
  }
  auto& visible=list.visible;
  visible.clear();list.quads=0;
  for (auto& [coordinate,column]:columns) {
    if (!column.face_count) continue;
    const float x=static_cast<float>(coordinate.first)*32,z=static_cast<float>(coordinate.second)*32;
    if (culling_enabled && !camera_is_box_visible(&visibility,x,static_cast<float>(column.min_y),z,32,static_cast<float>(column.height),32)) continue;
    const float dx=x+16-eye.x,dz=z+16-eye.z;
    visible.push_back({&column,dx*dx+dz*dz});
    list.quads+=column.face_count;
  }
  // Original forward owner submits transparent columns from farthest to nearest.
  std::stable_sort(visible.begin(),visible.end(),[](const WorldVisibleColumn& a,const WorldVisibleColumn& b){return a.distance>b.distance;});
}
const WorldVisibleColumn& world_draw_item(const WorldDrawList& list,std::size_t index,bool forward) {
  // Preserve far-to-near blending; opaque/sprites use the original near-to-far order.
  return list.visible[forward?index:list.visible.size()-1-index];
}
bool world_renderer_draw(WorldRenderer& r,rhi::IRenderPassEncoder* render,bool forward) {
  auto& uniforms=r.draw_uniforms;
  constexpr std::size_t order[]={0,1,2,4,3}; // Original opaque/sprite, glass, lava, water.
  for (const auto pass:order) {
    if((pass>=2)!=forward) continue;
    if(pass<2 && r.batch && r.batch->prepared) {
      if(!world_batch_draw(r,render,pass))return false;
      continue;
    }
    rhi::IShaderObject* root=nullptr;
    for (std::size_t index=0;index<r.draw_list.visible.size();++index) {
      const auto& item=world_draw_item(r.draw_list,index,forward);
      auto& column=*item.column;
      uniforms[28]=static_cast<float>(column.pass_counts[0]+column.pass_counts[1]+column.pass_counts[2]);
      if (!column.pass_counts[pass]) continue;
      if(!root) {
        root=render->bindPipeline(pass==0?r.raster_pipeline:pass==1?r.sprite_pipeline:
            pass==4?r.lava_pipeline:pass==3 && r.ray_enabled && world_ray_available(r)?r.ray_water_pipeline:r.transparent_pipeline);
        if(!root || !bind_world_atlas(r.atlas,root)) return false;
        if(pass==3 && r.ray_enabled && world_ray_available(r) && !world_ray_bind(r,root))return false;
      }
      // RHI snapshots bindings at each draw; reuse the pass root and shared atlas.
      if(!world_rhi_ok(root->setBinding({0,0,0},rhi::Binding(column.faces))) ||
         !world_rhi_ok(root->setBinding({0,7,0},rhi::Binding(column.fluids))) ||
         !world_rhi_ok(root->setBinding({0,8,0},rhi::Binding(column.patches))) ||
         !world_rhi_ok(root->setData({0,0,0},uniforms.data(),sizeof(uniforms)))) return false;
      render->drawIndirect(1,{column.arguments,80+pass*16});
      if(pass<2 && r.batch) {++r.batch->submitted_commands;++r.batch->submitted_columns;}
    }
  }
  return true;
}
}
