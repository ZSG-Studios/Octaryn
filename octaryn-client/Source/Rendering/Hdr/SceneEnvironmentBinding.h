#pragma once
#include <array>
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
template<class Renderer> std::array<float,4> scene_sun(const Renderer& r) {
  const auto& e=r.scene_environment;
  if(e.enabled)return {e.directional_direction[0],e.directional_direction[1],e.directional_direction[2],1};
  return {-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
}
template<class Renderer> bool bind_scene_environment(const Renderer& r,rhi::IShaderObject* root) {
  const auto& e=r.scene_environment;rhi::ShaderCursor c(root);
  const float ambient[4]={e.ambient[0],e.ambient[1],e.ambient[2],e.enabled?1.f:0.f};
  const float directional[4]={e.directional_color[0],e.directional_color[1],e.directional_color[2],e.sky_enabled?1.f:0.f};
  const float background[4]={e.background[0],e.background[1],e.background[2],0};
  const auto bind=[&](const char* name,const float* value) {
    auto field=c[name];return !field.isValid() || SLANG_SUCCEEDED(field.setData(value,16));
  };
  return bind("sceneAmbient",ambient) && bind("sceneDirectional",directional) && bind("sceneBackground",background);
}
}
