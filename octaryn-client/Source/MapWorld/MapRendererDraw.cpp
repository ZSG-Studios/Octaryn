#include "MapRendererInternal.h"
#include "Camera.h"
#include "CameraMatrix.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
  // metallic, roughness, alpha cutoff, first shared-buffer index.
  float material[4];
  float camera[4],right[4],up[4],forward[4],projection[4],light[4],settings[4];
};
static_assert(sizeof(MapDrawUniforms)==9*16);
bool bind_shared(MapRenderer& map,rhi::IShaderObject* root) {
  rhi::ShaderCursor cursor(root);
  return SLANG_SUCCEEDED(cursor["mapVertices"].setBinding(rhi::Binding(map.vertices.get()))) &&
      SLANG_SUCCEEDED(cursor["mapIndices"].setBinding(rhi::Binding(map.indices.get()))) &&
      SLANG_SUCCEEDED(cursor["mapSampler"].setBinding(rhi::Binding(map.sampler.get())));
}
rhi::ITextureView* primitive_texture(const MapRenderer& map,const MapMaterial& material) {
  return material.texture>=0 && static_cast<size_t>(material.texture)<map.texture_views.size()
      ?map.texture_views[static_cast<size_t>(material.texture)].get():map.white_view.get();
}
bool draw_primitive(MapRenderer& map,rhi::IRenderPassEncoder* pass,rhi::IShaderObject* root,
    const MapPrimitive& primitive,const WorldRenderer& r) {
  MapDrawUniforms uniforms{};
  const auto& draw=r.draw_uniforms;
  for(size_t k=0;k<4;++k)uniforms.base_color[k]=primitive.material.base_color[k];
  uniforms.material[0]=primitive.material.metallic;
  uniforms.material[1]=primitive.material.roughness;
  uniforms.material[2]=primitive.material.alpha_cutoff;
  uniforms.material[3]=static_cast<float>(primitive.first_index);
  std::copy_n(draw.begin()+20,4,uniforms.light);
  uniforms.settings[0]=draw[29];
  uniforms.settings[1]=draw[25];
  uniforms.settings[2]=draw[34];
  uniforms.settings[3]=0;
  std::copy_n(draw.begin(),4,uniforms.camera);
  std::copy_n(draw.begin()+4,4,uniforms.right);
  std::copy_n(draw.begin()+8,4,uniforms.up);
  std::copy_n(draw.begin()+12,4,uniforms.forward);
  std::copy_n(draw.begin()+16,4,uniforms.projection);
  rhi::ShaderCursor cursor(root);
  if(SLANG_FAILED(cursor["baseColorTexture"].setBinding(
      rhi::Binding(primitive_texture(map,primitive.material)))))return false;
  if(SLANG_FAILED(cursor["mapUniforms"].setData(&uniforms,sizeof(uniforms))))return false;
  rhi::DrawArguments draw_arguments{};
  draw_arguments.vertexCount=primitive.index_count;
  pass->draw(draw_arguments);
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
    const WorldRenderer& r,bool forward) {
  if(!renderer || !pass)return false;
  auto& map=*renderer;
  const auto visibility=map_camera(eye,r);
  rhi::IShaderObject* root=nullptr;
  if(forward) {
    order_forward_blends(map,eye);
    for(const auto index:map.forward_order) {
      const auto& primitive=map.model.primitives[index];
      if(!visible(visibility,primitive))continue;
      if(!root) {
        root=pass->bindPipeline(map.forward_pipeline);
        if(!root || !bind_shared(map,root))return false;
      }
      if(!draw_primitive(map,pass,root,primitive,r))return false;
    }
    return true;
  }
  for(std::uint32_t index=0;index<map.model.primitives.size();++index) {
    const auto& primitive=map.model.primitives[index];
    if(primitive.material.alpha_mode==MapAlphaMode::Blend)continue;
    if(!visible(visibility,primitive))continue;
    if(!root) {
      root=pass->bindPipeline(map.gbuffer_pipeline);
      if(!root || !bind_shared(map,root))return false;
    }
    if(!draw_primitive(map,pass,root,primitive,r))return false;
  }
  return true;
}
}
