#include "MapDrawBinding.h"
#include "MapRendererInternal.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
namespace octaryn::client::rendering {
struct MapDrawUniforms {
  float base_color[4];
  // Metallic, roughness, alpha cutoff.
  float material[4];
  float camera[4],right[4],up[4],forward[4],projection[4],light[4],settings[4];
  float sun[4],sky[4];
};
static_assert(sizeof(MapDrawUniforms)==11*16);
bool bind_map_geometry(MapRenderer& map,rhi::IShaderObject* root) {
  rhi::ShaderCursor cursor(root);
  return SLANG_SUCCEEDED(cursor["mapVertices"].setBinding(rhi::Binding(map.vertices.get()))) &&
      (!cursor["mapIndices"].isValid() || SLANG_SUCCEEDED(cursor["mapIndices"].setBinding(rhi::Binding(map.indices.get())))) &&
      SLANG_SUCCEEDED(cursor["mapRayPrimitives"].setBinding(rhi::Binding(map.ray_primitives.get(),map.material_buffer_range)));
}
bool bind_map_draw_uniforms(MapRenderer& map,rhi::IShaderObject* root,
    const MapPrimitive& primitive,const WorldRenderer& r,const virtual_geometry::GeometryTransform* instance) {
  MapDrawUniforms uniforms{};
  const auto& draw=r.view_uniforms;
  for(size_t k=0;k<4;++k)uniforms.base_color[k]=primitive.material.base_color[k];
  uniforms.material[0]=primitive.material.metallic;
  uniforms.material[1]=primitive.material.roughness;
  uniforms.material[2]=primitive.material.alpha_cutoff;
  uniforms.material[3]=0;
  for(size_t k=0;k<3;++k)uniforms.light[k]=-r.sky.light_direction_sky[k];
  uniforms.light[3]=r.lighting.gameplay_sky_visibility;
  uniforms.settings[0]=r.pbr?1.f:0.f;
  uniforms.settings[1]=r.pom?1.f:0.f;
  uniforms.settings[2]=r.lighting.skylight_floor;
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
  const virtual_geometry::GeometryTransform identity;const auto& transform=instance?*instance:identity;
  if(cursor["mapWorld"].isValid() && SLANG_FAILED(cursor["mapWorld"].setData(transform.world.data(),sizeof(transform.world))))return false;
  if(cursor["mapNormal"].isValid() && SLANG_FAILED(cursor["mapNormal"].setData(transform.normal.data(),sizeof(transform.normal))))return false;
  if(cursor["mapOrientation"].isValid() && SLANG_FAILED(cursor["mapOrientation"].setData(&transform.orientation,sizeof(transform.orientation))))return false;
  if(SLANG_FAILED(cursor["mapUniforms"].setData(&uniforms,sizeof(uniforms))))return false;
  return true;
}
}
