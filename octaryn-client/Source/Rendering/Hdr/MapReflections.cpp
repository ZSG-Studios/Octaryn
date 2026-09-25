#include "MapReflections.h"
#include "ReflectionQuality.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>

namespace octaryn::client::rendering {
namespace {
bool image(rhi::IDevice* device,ReflectionImage& image,unsigned width,unsigned height,
    rhi::Format format,const char* label) {
  image={};rhi::TextureDesc desc{};
  desc.size={width,height,1};desc.format=format;desc.label=label;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  return world_rhi_ok(device->createTexture(desc,nullptr,image.texture.writeRef())) &&
      world_rhi_ok(image.texture->getDefaultView(image.view.writeRef()));
}
bool data(rhi::ShaderCursor cursor,const char* name,const float* value) {
  return world_rhi_ok(cursor[name].setData(value,4*sizeof(float)));
}
bool bind(rhi::ShaderCursor cursor,const char* name,rhi::ITextureView* view) {
  return world_rhi_ok(cursor[name].setBinding(view));
}
bool close_state(const std::array<float,4>& a,const std::array<float,4>& b) {
  // Sky time and sun direction are continuously integrated floats. Treat tiny
  // per-frame drift as the same lighting epoch so history can converge.
  return std::abs(a[0]-b[0])<=.01f && std::abs(a[1]-b[1])<=.01f &&
      std::abs(a[2]-b[2])<=.01f && std::abs(a[3]-b[3])<=.01f;
}
bool scene_stable_for_reflections(const WorldRenderer& r,std::uint64_t cursor) {
  if(cursor==r.scene_changes.revision())return true;
  bool stable=true;
  // Sprite-only/acceleration bookkeeping must not flush glossy history.  A
  // full opaque rebuild still invalidates history because a reflected ray can
  // now hit different geometry.  Keep this decision on the render thread so
  // the shader receives a coherent history epoch.
  if(!r.scene_changes.for_each_since(cursor,[&](const SceneChange& change) {
       if(!change.minor_build)stable=false;
     }))return false;
  return stable;
}
}
bool prepare_map_reflections(WorldRenderer& r) {
  auto& s=r.map_reflections;
  const char* option=std::getenv("OCTARYN_CLIENT_MAP_REFLECTION_TEMPORAL");
  // The repaired reprojection path is the normal map resolve. Keep the
  // original four-ray path available as an explicit quality/debug fallback.
  s.enabled=!(option && *option=='0');
  if(!r.map || !s.enabled || !r.device->hasFeature(rhi::Feature::RayQuery))return true;
  if(!s.resolve && !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Hdr/MapReflectionTemporal.slang","main",s.resolve))return false;
  const auto quality=reflection_quality(r.lighting_settings.reflection_quality);
  const unsigned width=reflection_extent(unsigned(r.render_width()),quality.divisor);
  const unsigned height=reflection_extent(unsigned(r.render_height()),quality.divisor);
  if(width==s.width && height==s.height)return true;
  // Reallocation may retire history still consumed by either frame slot.
  if(!r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms()))return false;
  for(auto& history:s.history) {
    if(!image(r.device,history.radiance,width,height,rhi::Format::RGBA16Float,"map_reflection_history") ||
        !image(r.device,history.position,width,height,rhi::Format::R32Float,"map_reflection_depth") ||
        !image(r.device,history.surface,width,height,rhi::Format::RGBA16Float,"map_reflection_surface") ||
        !image(r.device,history.material,width,height,rhi::Format::RGBA8Unorm,"map_reflection_material"))return false;
  }
  s.width=width;s.height=height;s.valid=false;s.pending=false;s.samples=0;s.index=0;
  s.camera.invalidate();
  std::printf("map_reflections temporal=1 width=%u height=%u tier=%u rays_per_pixel=%u directions=%u history_max=%u\n",
      width,height,r.lighting_settings.reflection_quality,quality.fresh_samples,quality.directions,quality.history_frames);
  std::fflush(stdout);return true;
}
bool render_map_reflections(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.map_reflections;s.pending=false;
  if(!r.map || !s.enabled || !r.ray_effects || !r.ray_enabled || r.lighting_settings.reflection_distance<=0) {
    s.valid=false;s.camera.invalidate();return true;
  }
  if(!prepare_map_reflections(r))return false;
  auto camera=r.temporal.camera;
  if(r.temporal.mode) {
    camera.jitter_x=2*r.temporal.jitter.x/float(r.render_width());
    camera.jitter_y=-2*r.temporal.jitter.y/float(r.render_height());
  }
  auto& hdr=r.target().hdr;
  const float eye[4]={camera.x,camera.y,camera.z,0};
  const float sun[4]={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
  const float lighting[4]={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0};
  const std::array<float,4> sun_state{sun[0],sun[1],sun[2],sun[3]};
  const std::array<float,4> lighting_state{lighting[0],lighting[1],lighting[2],lighting[3]};
  const bool valid=s.valid && s.range==r.lighting_settings.reflection_distance &&
      s.quality==r.lighting_settings.reflection_quality && s.light_revision==r.local_lighting.light_revision &&
      s.gi_epoch==r.block_gi.epoch && s.gi_active==r.block_gi.active &&
      scene_stable_for_reflections(r,s.scene_revision) &&
      close_state(s.last_sun,sun_state) && close_state(s.last_lighting,lighting_state) &&
      !s.camera.reset(camera,int(s.width),int(s.height),1.0/60);
  const unsigned output=1-s.index;
  auto& current=s.history[output];auto& previous=s.history[s.index];
  // Every active pixel is overwritten; invalid history is never read. Clearing
  // all eight images on a lighting change adds bandwidth and barriers only.
  float dimensions[4]={float(s.width),float(s.height),float(s.samples%4096),0};
  const float source_dimensions[4]={float(r.render_width()),float(r.render_height()),0,0};
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(s.resolve);
  bool ok=root && world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && bind_block_transport_lookup(r,root);
  if(ok) {
    rhi::ShaderCursor c(root);
    const auto prior=temporal_view(valid?s.camera.previous():camera,int(s.width),int(s.height));
    const auto current_view=temporal_view(camera,int(s.width),int(s.height));
    const auto& old=valid?s.camera.previous():camera;
    const float jitter[4]={old.jitter_x,old.jitter_y,0,0};
    dimensions[3]=valid?1.f:0.f;
    ok=bind(c,"colors",hdr.views[0]) && bind(c,"positions",hdr.views[1]) &&
        bind(c,"voxels",hdr.views[2]) && bind(c,"materials",hdr.views[3]) &&
        bind(c,"previousRadiance",previous.radiance.view) && bind(c,"previousPosition",previous.position.view) &&
        bind(c,"previousSurface",previous.surface.view) && bind(c,"previousMaterial",previous.material.view) &&
        bind(c,"reflectionHistory",current.radiance.view) && bind(c,"reflectionPosition",current.position.view) &&
        bind(c,"reflectionSurface",current.surface.view) && bind(c,"reflectionMaterial",current.material.view) &&
        data(c,"eye",eye) && data(c,"dimensions",dimensions) && data(c,"sourceDimensions",source_dimensions) &&
        data(c,"sun",sun) && data(c,"lighting",lighting) && data(c,"previousJitter",jitter) &&
        data(c,"previousPositionCamera",prior.position.data()) && data(c,"previousRight",prior.right.data()) &&
        data(c,"previousUp",prior.up.data()) && data(c,"previousForward",prior.forward.data()) &&
        data(c,"previousProjection",prior.projection.data()) &&
        data(c,"currentForward",current_view.forward.data());
  }
  if(ok)pass->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
  pass->end();if(!ok)return false;
  for(auto* image:{&current.radiance,&current.position,&current.surface,&current.material})
    commands->setTextureState(image->texture,rhi::ResourceState::ShaderResource);
  s.pending=true;s.pending_index=output;s.pending_camera=camera;
  return true;
}
void commit_map_reflections(WorldRenderer& r) {
  auto& s=r.map_reflections;if(!s.pending)return;
  s.camera.commit(s.pending_camera,int(s.width),int(s.height));
  s.index=s.pending_index;s.valid=true;s.pending=false;++s.samples;
  s.range=r.lighting_settings.reflection_distance;
  s.quality=r.lighting_settings.reflection_quality;
  s.light_revision=r.local_lighting.light_revision;
  s.gi_epoch=r.block_gi.epoch;s.gi_active=r.block_gi.active;
  s.scene_revision=r.scene_changes.revision();
  s.last_sun={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
  s.last_lighting={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0};
}
}
