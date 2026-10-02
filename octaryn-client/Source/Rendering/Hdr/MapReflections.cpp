#include "SceneEnvironmentBinding.h"
#include "MapReflections.h"
#include "ReflectionQuality.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>

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
// The queue is an experimental opt-in path: a failed prepare or dispatch must
// degrade to the fused temporal resolve for this and later frames.
void disable_map_reflection_queue(WorldRenderer& r,const std::string& prior_status) {
  auto& s=r.map_reflections;
  const std::string reason=r.status!=prior_status?r.status:std::string("queued_reflection_resources_unavailable");
  s.queue_disabled=true;s.queue.enabled=false;r.status=prior_status;
  std::printf("map_reflection_queue disabled=1 reason=%s fallback=fused\n",reason.c_str());
  std::fflush(stdout);
}
}
bool prepare_map_reflections(WorldRenderer& r) {
  auto& s=r.map_reflections;
  const char* map_only=std::getenv("OCTARYN_CLIENT_RT_MAP_ONLY");
  if(map_only && std::strcmp(map_only,"0") && std::strcmp(map_only,"1")) {
    r.status="invalid_map_only_reflection_mode";return false;
  }
  bool qualified_default=false;
#ifdef _WIN32
  const auto backend=r.device->getInfo().deviceType;
  qualified_default=backend==rhi::DeviceType::D3D12 || backend==rhi::DeviceType::Vulkan;
#endif
  const char* reference=std::getenv("OCTARYN_CLIENT_RT_REFERENCE");
  s.reference=reference && *reference=='1';
  const char* full_fresh=std::getenv("OCTARYN_CLIENT_RT_TEMPORAL_FULL_FRESH");
  s.full_fresh=full_fresh && *full_fresh=='1';
  const char* sparse=std::getenv("OCTARYN_CLIENT_RT_SPARSE");
  s.sparse=!s.reference && sparse && *sparse=='1';
  const char* search=std::getenv("OCTARYN_CLIENT_RT_HISTORY_SEARCH");
  s.history_search=!s.reference && search && *search=='1';
  const char* option=std::getenv("OCTARYN_CLIENT_MAP_REFLECTION_TEMPORAL");
  // The repaired reprojection path is the normal map resolve. Keep the
  // original four-ray path available as an explicit quality/debug fallback.
  s.enabled=!(option && *option=='0');
  const char* queued=std::getenv("OCTARYN_CLIENT_RT_QUEUED");
  const char* deferred=std::getenv("OCTARYN_CLIENT_RT_DEFERRED_MATERIAL");
  if(deferred && std::strcmp(deferred,"0") && std::strcmp(deferred,"1")) {
    r.status="invalid_deferred_reflection_material_mode";return false;
  }
  const bool previous_deferred=s.deferred_material;
  s.map_only=map_only?std::strcmp(map_only,"1")==0:
      qualified_default && s.enabled && !(queued && std::strcmp(queued,"1")==0);
  if(s.map_only && (!s.enabled || (queued && std::strcmp(queued,"1")==0))) {
    r.status="map_only_reflections_require_temporal_without_queue";return false;
  }
  const bool deferred_eligible=s.map_only && s.enabled && r.ray_effects &&
      !r.block_transport_lookup.active && r.lighting_settings.reflection_distance>0;
  s.deferred_material=deferred?std::strcmp(deferred,"1")==0:qualified_default && deferred_eligible;
  if(s.deferred_material && !deferred_eligible) {
    r.status="deferred_reflections_require_direct_map_traversal";return false;
  }
  if(previous_deferred!=s.deferred_material) {s.resolve.setNull();s.valid=false;}
  const char* quad=std::getenv("OCTARYN_CLIENT_RT_ROUGH_QUAD");
  if(quad && std::strcmp(quad,"0")!=0) {
    r.status="rough_reflection_quad_retired_after_performance_regression";return false;
  }
  const char* witness=std::getenv("OCTARYN_CLIENT_RT_VISIBILITY_WITNESS");
  if(witness && std::strcmp(witness,"0")!=0) {
    r.status="reflection_visibility_witness_retired_after_performance_regression";return false;
  }
  if(!r.reflection_wave.path(s.map_only,s.enabled,queued && std::strcmp(queued,"1")==0,
      r.ray_effects,r.device->hasFeature(rhi::Feature::RayQuery),r.lighting_settings.reflection_distance)) {
    r.status="reflection_wave_requires_active_fused_map_temporal_reflections";return false;
  }
  if(!r.map || !s.enabled || !r.device->hasFeature(rhi::Feature::RayQuery))return true;
  const char* shader=s.deferred_material?"octaryn-client/Shaders/Hdr/MapReflectionTemporalDeferred.slang":
      s.map_only?"octaryn-client/Shaders/Hdr/MapReflectionTemporalMap.slang":
      "octaryn-client/Shaders/Hdr/MapReflectionTemporal.slang";
  if(!s.resolve && !create_rhi_compute_pipeline(r.device,shader,"main",s.resolve))return false;
  if(!s.filter && !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Hdr/MapReflectionFilter.slang","main",s.filter))return false;
  if(!s.classify && !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Hdr/MapReflectionTiles.slang","main",s.classify))return false;
  const auto quality=reflection_quality(r.lighting_settings.reflection_quality);
  const unsigned width=reflection_extent(unsigned(r.render_width()),quality.divisor);
  const unsigned height=reflection_extent(unsigned(r.render_height()),quality.divisor);
  const unsigned maximum_width=r.temporal.mode?r.temporal.allocation_width:unsigned(r.width);
  const unsigned maximum_height=r.temporal.mode?r.temporal.allocation_height:unsigned(r.height);
  const unsigned allocation_width=std::max(s.allocation_width,reflection_extent(maximum_width,quality.divisor));
  const unsigned allocation_height=std::max(s.allocation_height,reflection_extent(maximum_height,quality.divisor));
  const std::string status_before_queue=r.status;
  if(!prepare_map_reflection_queue(r,allocation_width,allocation_height))
    disable_map_reflection_queue(r,status_before_queue);
  // Fused-path marker; queue-active runs report through map_reflection_queue lines.
  if(!r.reflection_wave.reported && !s.queue.enabled) {
    std::printf("reflection_wave_path requested=%u map_only=%u temporal=1 queued=%u applied=%u\n",
        r.reflection_wave.requested,unsigned(s.map_only),unsigned(s.queue.enabled),r.reflection_wave.requested);
    r.reflection_wave.reported=true;
  }
  if(width!=s.width || height!=s.height) {
    s.width=width;s.height=height;s.valid=false;s.pending=false;s.camera.invalidate();
  }
  if(allocation_width==s.allocation_width && allocation_height==s.allocation_height)return true;
  // Reallocation may retire history still consumed by either frame slot.
  if(!r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms()))return false;
  for(auto& history:s.history) {
    if(!image(r.device,history.moments,allocation_width,allocation_height,rhi::Format::RG16Float,"map_reflection_moments") ||
        !image(r.device,history.radiance,allocation_width,allocation_height,rhi::Format::RGBA16Float,"map_reflection_history") ||
        !image(r.device,history.position,allocation_width,allocation_height,rhi::Format::R32Float,"map_reflection_depth") ||
        !image(r.device,history.surface,allocation_width,allocation_height,rhi::Format::RGBA16Float,"map_reflection_surface") ||
        !image(r.device,history.material,allocation_width,allocation_height,rhi::Format::RGBA8Unorm,"map_reflection_material"))return false;
  }
  if(!image(r.device,s.filtered,allocation_width,allocation_height,rhi::Format::RGBA16Float,"map_reflection_filtered"))return false;
  if(!image(r.device,s.tiles,(allocation_width+7)/8,(allocation_height+7)/8,rhi::Format::R32Uint,"map_reflection_tiles"))return false;
  s.allocation_width=allocation_width;s.allocation_height=allocation_height;
  s.width=width;s.height=height;s.valid=false;s.pending=false;s.samples=0;s.index=0;
  s.camera.invalidate();
  std::printf("map_reflections temporal=1 width=%u height=%u tier=%u rays_per_pixel=%u directions=%u history_max=%u\n",
      width,height,r.lighting_settings.reflection_quality,quality.fresh_samples,quality.directions,quality.history_frames);
  if(s.full_fresh)std::printf("map_reflections temporal_full_fresh=1 diagnostic=1\n");
  std::printf("map_reflections map_only=%u sampling_unchanged=1 qualified_default=%u\n",
      s.map_only?1u:0u,qualified_default?1u:0u);
  std::printf("map_reflections deferred_material=%u direct_gi=%u qualified_default=%u\n",
      s.deferred_material?1u:0u,r.block_transport_lookup.active?0u:1u,unsigned(qualified_default && deferred_eligible));
  std::fflush(stdout);return true;
}
bool render_map_reflections(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.map_reflections;s.pending=false;
  if(r.reflection_wave.requested && (!r.map || !s.enabled || !r.ray_effects || r.lighting_settings.reflection_distance<=0)) {
    r.status="reflection_wave_requires_active_fused_map_temporal_reflections";return false;
  }
  if(!r.map || !s.enabled || !r.ray_effects || !r.ray_enabled || r.lighting_settings.reflection_distance<=0) {
    s.valid=false;s.camera.invalidate();return true;
  }
  if(!prepare_map_reflections(r))return false;
  if(s.map_only && !world_ray_triangle_scene(r)) {
    r.status="map_only_reflections_require_triangle_scene";return false;
  }
  auto camera=r.temporal.camera;
  if(r.temporal.mode) {
    camera.jitter_x=2*r.temporal.jitter.x/float(r.render_width());
    camera.jitter_y=-2*r.temporal.jitter.y/float(r.render_height());
  }
  auto& hdr=r.target().hdr;
  const float eye[4]={camera.x,camera.y,camera.z,0};
  const auto sun=scene_sun(r);
  const float lighting[4]={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0};
  const std::array<float,4> sun_state{sun[0],sun[1],sun[2],sun[3]};
  const std::array<float,4> lighting_state{lighting[0],lighting[1],lighting[2],lighting[3]};
  const bool valid=s.valid && s.range==r.lighting_settings.reflection_distance &&
      s.source_width==unsigned(r.render_width()) && s.source_height==unsigned(r.render_height()) &&
      s.quality==r.lighting_settings.reflection_quality && s.light_revision==r.local_lighting.light_revision &&
      s.gi_epoch==r.block_transport_lookup.epoch && s.gi_active==r.block_transport_lookup.active &&
      scene_stable_for_reflections(r,s.scene_revision) &&
      close_state(s.last_sun,sun_state) && close_state(s.last_lighting,lighting_state) &&
      !s.camera.reset(camera,int(s.width),int(s.height),1.0/60);
  const unsigned output=1-s.index;
  auto& current=s.history[output];auto& previous=s.history[s.index];
  // Every active pixel is overwritten; invalid history is never read. Clearing
  // all eight images on a lighting change adds bandwidth and barriers only.
  const auto sample_frame=r.temporal.fixed_sampling?r.temporal.validation_frame:s.samples;
  r.temporal.reflection_sampling_frame=static_cast<std::int64_t>(sample_frame%4096);
  float dimensions[4]={float(s.width),float(s.height),float(sample_frame%4096),0};
  const float source_dimensions[4]={float(r.render_width()),float(r.render_height()),0,0};
  if(s.sparse && valid && !s.queue.enabled) {
    r.lighting_profile.begin_pass(commands,LightingPass::ReflectionClassify);
    auto* classify=commands->beginComputePass();if(!classify)return false;
    auto* root=classify->bindPipeline(s.classify);bool ok=root!=nullptr;
    if(ok) {
      rhi::ShaderCursor c(root);
      ok=bind(c,"previousRadiance",previous.radiance.view) && bind(c,"previousSurface",previous.surface.view) &&
          bind(c,"previousMoments",previous.moments.view) && bind(c,"tiles",s.tiles.view) && data(c,"dimensions",dimensions);
    }
    if(ok)classify->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
    classify->end();if(!ok)return false;
    commands->setTextureState(s.tiles.texture,rhi::ResourceState::ShaderResource);
    r.lighting_profile.mark(commands,LightingPass::ReflectionClassify);
  }
  rhi::IComputePassEncoder* pass=nullptr;
  rhi::IShaderObject* root=nullptr;
  bool ok=true;
  const std::string status_before_queue=r.status;
  if(s.queue.enabled) {
    dimensions[3]=valid?1.f:0.f;
    if(!render_map_reflection_coverage(r,commands) || !render_map_reflection_queue(r,commands,valid,dimensions))
      disable_map_reflection_queue(r,status_before_queue);
  }
  if(!s.queue.enabled) {
  r.lighting_profile.begin_pass(commands,LightingPass::ReflectionTrace);
  pass=commands->beginComputePass();if(!pass)return false;
  root=pass->bindPipeline(s.resolve);
  ok=root && bind_scene_environment(r,root) && world_ray_bind(r,root) && bind_world_atlas(r.atlas,root) && bind_block_transport_lookup(r,root);
  if(ok) {
    rhi::ShaderCursor c(root);
    const unsigned reference=s.reference?1u:0u;
    const unsigned sparse=s.sparse && valid?1u:0u;
    const unsigned search=s.history_search?1u:0u;
    const unsigned full_fresh=s.full_fresh?1u:0u;
    const auto prior=temporal_view(valid?s.camera.previous():camera,r.render_width(),r.render_height());
    const auto current_view=temporal_view(camera,int(s.width),int(s.height));
    const auto& old=valid?s.camera.previous():camera;
    const float jitter[4]={old.jitter_x,old.jitter_y,0,0};
    dimensions[3]=valid?1.f:0.f;
    ok=world_rhi_ok(c["referenceMode"].setData(&reference,sizeof(reference))) &&
        world_rhi_ok(c["fullFreshDiagnostic"].setData(&full_fresh,sizeof(full_fresh))) &&
        world_rhi_ok(c["historySearch"].setData(&search,sizeof(search))) &&
        world_rhi_ok(c["sparseMode"].setData(&sparse,sizeof(sparse))) && bind(c,"reflectionTiles",s.tiles.view) &&
        bind(c,"previousMoments",previous.moments.view) && bind(c,"reflectionMoments",current.moments.view) &&
        bind(c,"colors",hdr.views[0]) && bind(c,"positions",hdr.views[1]) &&
        bind(c,"voxels",hdr.views[2]) && bind(c,"materials",hdr.views[3]) &&
        bind(c,"previousRadiance",previous.radiance.view) && bind(c,"previousPosition",previous.position.view) &&
        bind(c,"previousSurface",previous.surface.view) && bind(c,"previousMaterial",previous.material.view) &&
        bind(c,"reflectionHistory",current.radiance.view) && bind(c,"reflectionPosition",current.position.view) &&
        bind(c,"reflectionSurface",current.surface.view) && bind(c,"reflectionMaterial",current.material.view) &&
        data(c,"eye",eye) && data(c,"dimensions",dimensions) && data(c,"sourceDimensions",source_dimensions) &&
        data(c,"sun",sun.data()) && data(c,"lighting",lighting) && data(c,"previousJitter",jitter) &&
        data(c,"previousPositionCamera",prior.position.data()) && data(c,"previousRight",prior.right.data()) &&
        data(c,"previousUp",prior.up.data()) && data(c,"previousForward",prior.forward.data()) &&
        data(c,"previousProjection",prior.projection.data()) &&
        data(c,"currentForward",current_view.forward.data());
  }
  if(ok)pass->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
  pass->end();if(!ok)return false;
  r.lighting_profile.mark(commands,LightingPass::ReflectionTrace);
  }
  for(auto* image:{&current.radiance,&current.position,&current.surface,&current.material,&current.moments})
    commands->setTextureState(image->texture,rhi::ResourceState::ShaderResource);
  if(!s.reference) {
    r.lighting_profile.begin_pass(commands,LightingPass::ReflectionFilter);
    pass=commands->beginComputePass();if(!pass)return false;
    root=pass->bindPipeline(s.filter);ok=root!=nullptr;
    if(ok) {
      rhi::ShaderCursor c(root);
      ok=bind(c,"radiance",current.radiance.view) && bind(c,"depths",current.position.view) &&
          bind(c,"surfaces",current.surface.view) && bind(c,"materials",current.material.view) &&
          bind(c,"moments",current.moments.view) && bind(c,"filtered",s.filtered.view) &&
          data(c,"dimensions",dimensions);
    }
    if(ok)pass->dispatchCompute((s.width+7)/8,(s.height+7)/8,1);
    pass->end();if(!ok)return false;
    r.lighting_profile.mark(commands,LightingPass::ReflectionFilter);
    commands->setTextureState(s.filtered.texture,rhi::ResourceState::ShaderResource);
  }
  s.pending=true;s.pending_index=output;s.pending_camera=camera;
  return true;
}
void commit_map_reflections(WorldRenderer& r) {
  auto& s=r.map_reflections;if(!s.pending)return;
  s.camera.commit(s.pending_camera,int(s.width),int(s.height));
  s.index=s.pending_index;s.valid=true;s.pending=false;++s.samples;
  s.range=r.lighting_settings.reflection_distance;
  s.quality=r.lighting_settings.reflection_quality;
  s.source_width=unsigned(r.render_width());s.source_height=unsigned(r.render_height());
  s.light_revision=r.local_lighting.light_revision;
  s.gi_epoch=r.block_transport_lookup.epoch;s.gi_active=r.block_transport_lookup.active;
  s.scene_revision=r.scene_changes.revision();
  s.last_sun=scene_sun(r);
  s.last_lighting={r.lighting.visual_sky_visibility,r.lighting.ambient_strength,r.sky.twilight_celestial_time[0],0};
}
}
