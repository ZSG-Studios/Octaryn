#include "WorldRendererInternal.h"
#include "WorldFrame.h"
#include "FrameCpuTrace.h"
#include "Camera.h"
#include "LightingSystem.h"
#include "FrameWatchdog.h"
#include "WorldAsTiming.h"
#include "FrameSubmissionGuard.h"
#include <algorithm>
#include <chrono>
#include <memory>
namespace octaryn::client::rendering {
bool render_world_frame(WorldRenderer& r,const WorldCamera& source_camera,FrameCpuTrace& trace) {
  const auto frame_start=std::chrono::steady_clock::now();
  r.active_frame=r.frame_queue.slot(r.frames);
  trace.begin("frame_fence");
  const auto wait_start=std::chrono::steady_clock::now();
  if(!r.frame_queue.wait(r.active_frame,frame_fence_timeout_ms(),trace.enabled()?&trace:nullptr)) {
    std::fprintf(stderr,"world_fence_timeout slot=%u timeout_ms=%llu\n",r.active_frame,
        static_cast<unsigned long long>(frame_fence_timeout_ms()));
    r.status="fence_timeout";return trace.failed();
  }
  const auto wait_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wait_start).count();
  trace.begin("gpu_counter_poll");
  if(r.gpu_counters)r.gpu_counters->poll();
  trace.begin("lighting_query_resolve");
  if(!r.lighting_profile.resolve(r.active_frame))return trace.failed();
  trace.begin("lighting_query_begin");
  if(!r.lighting_profile.begin(r.active_frame))return trace.failed();
  r.frame_fail_stage="frame_head";
  trace.begin("gpu_query_resolve");
  if(r.gpu_profile) {
    if(!r.gpu_profile->resolve(r.active_frame))return trace.failed();
    r.gpu_profile->begin_cpu(r.active_frame,wait_ms);
  }
  trace.begin("temporal_query_resolve");
  float temporal_gpu_ms{};
  if(!r.temporal.timing.resolve(r.active_frame,temporal_gpu_ms))return trace.failed();
  trace.begin("temporal_resize");
  if(r.temporal.resolution.sample(temporal_gpu_ms))update_temporal_size(r.temporal);
  auto& target=r.target();
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("encoder_create");
  r.frame_fail_stage="atlas_encoder";
  auto commands=r.queue->createCommandEncoder();
  if(!commands)return trace.failed();
  trace.begin("gpu_query_begin");
  if(r.gpu_profile && !r.gpu_profile->begin(commands))return trace.failed();
  trace.begin("temporal_query_begin");
  if(r.temporal.resolution.active && !r.temporal.timing.begin(commands,r.active_frame))return trace.failed();
  trace.begin("ray_counter_resolve_and_begin");
  if(!r.ray_diagnostics.begin(r.device,commands,r.active_frame,r.frames))return trace.failed();
  trace.begin("atlas_update");
  if(!update_world_atlas(r.atlas,commands,static_cast<double>(SDL_GetTicks())/1000.0))return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("temporal_begin");
  const auto camera=begin_temporal(r.temporal,source_camera,r.frames);
  WorldAsTiming as_timing{r.gpu_profile.get(),r.temporal.resolution.active?&r.temporal.timing:nullptr,r.active_frame};
  const auto as_scope=as_timing.scope();
  FrameSubmissionGuard submission_guard(as_timing.started,
      [&] {return r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms());},
      [] {frame_gpu_shutdown_failed("abandoned_frame_as");});
  trace.begin("map_ray_lifecycle");
  r.frame_fail_stage="map_ray_lifecycle";
  bool maps_ready=true;
  for(const auto& map:r.resident_maps) {
    if(!r.tile_session && !pump_map_ray_scene(*map,r.queue.get(),r.ray_requested && world_ray_available(r),&as_scope))return trace.failed();
    maps_ready=maps_ready && map_ray_ready(*map);
  }
  r.ray_enabled=r.ray_requested && maps_ready;
  if(r.tile_session) {
    trace.begin("tile_pump");
    r.frame_fail_stage="tile_pump";
    const auto generation=r.tile_session->stats().generation;
    if(!r.tile_session->pump(camera,r.tile_anchor_valid?r.tile_anchor:source_camera,commands,
        r.ray_requested && world_ray_available(r),r.resident_maps,&as_scope)) {
      r.status=r.tile_session->error();return trace.failed();
    }
    r.map=r.resident_maps.empty()?nullptr:r.resident_maps.front().get();
    if(generation!=r.tile_session->stats().generation) {
      refresh_resident_texture_bytes(r);
      r.scene_changes.notify_column(0,0,0,0,SceneChangeKind::Modified);
    }
  }
  // Acquisition reserves WSI semaphores for the next normal queue submission.
  // All independent AS submissions must precede that reservation.
  Slang::ComPtr<rhi::ITexture> image;
  trace.begin("surface_acquire");
  if(!world_rhi_ok(r.surface->acquireNextImage(image.writeRef())))return trace.failed();
  if(!image) {
    trace.begin("surface_resize_upload_submit");
    // Tile upload offsets already advanced while recording. Preserve those
    // uploads and fence their resources even though this frame has no image.
    auto uploads=commands->finish();
    if(!uploads || !r.frame_queue.submit(r.queue,uploads,r.active_frame,r.frames))return trace.failed();
    submission_guard.submitted();
    if(r.tile_session)r.tile_session->submitted(r.frame_queue.fence(),r.frame_queue.last_signal());
    commit_world_atlas(r.atlas);
    std::fprintf(stdout,"world_frame_abandoned frame=%llu reason=surface_resize uploads_submitted=1 external_as=%u timing_reported=0\n",
        static_cast<unsigned long long>(r.frames),unsigned(as_timing.started));
    trace.begin("surface_resize");
    const bool resized=world_renderer_resize(r,r.width,r.height);
    trace.finish("abandoned");return resized;
  }
  r.camera_position[0]=camera.x;r.camera_position[1]=camera.y;r.camera_position[2]=camera.z;
  const int render_width=r.render_width(),render_height=r.render_height();
  {
    const float sy=std::sin(camera.yaw),cy=std::cos(camera.yaw),sp=std::sin(camera.pitch),cp=std::cos(camera.pitch);
    const float focal=1/std::tan(std::clamp(camera.vertical_fov,.2f,2.7f)/2);
    constexpr float near_plane=.1f,far_plane=8192;
    r.view_uniforms={camera.x,camera.y,camera.z,0,cy,0,sy,camera.jitter_x,-sy*sp,cp,cy*sp,camera.jitter_y,
      sy*cp,sp,-cy*cp,0,focal*static_cast<float>(render_height)/static_cast<float>(render_width),focal,
      far_plane/(far_plane-near_plane),near_plane*far_plane/(far_plane-near_plane)};
  }
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  r.frame_fail_stage="encoder";
  r.lighting_profile.begin_pass(commands,LightingPass::Acceleration);
  if(!prepare_item_instances(r,commands))return trace.failed();
  trace.begin("world_ray_prepare");
  r.frame_fail_stage="ray_prepare";
  if(!world_ray_prepare(r,commands,r.active_frame))return trace.failed();
  trace.begin("target_init");
  r.frame_fail_stage="ray_profile_mark";
  r.lighting_profile.mark(commands,LightingPass::Acceleration);
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  r.frame_fail_stage="target_init";
  if(!target.initialized) {
    float clear[4]{};
    commands->clearTextureFloat(target.hdr.scene,{0,1,0,1},clear);
    commands->clearTextureFloat(target.color,{0,1,0,1},clear);
    float visible[4]={1,1,1,1};
    commands->clearTextureFloat(target.hdr.sun_visibility,{0,1,0,1},visible);
    commands->setTextureState(target.hdr.sun_visibility,rhi::ResourceState::ShaderResource);
    if(r.temporal.mode) {
      auto& temporal=r.temporal.targets[r.active_frame];
      for(auto* texture:{temporal.opaque.get(),temporal.motion.get(),temporal.reactive.get()}) {
        commands->clearTextureFloat(texture,{0,1,0,1},clear);
        // Order initialization before later copies, including same-layout Vulkan writes.
        commands->setTextureState(texture,rhi::ResourceState::ShaderResource);
      }
    }
  }
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("sky_encode");
  rhi::RenderPassDepthStencilAttachment depth{};
  depth.view=target.depth_view;depth.depthClearValue=1;depth.depthLoadOp=rhi::LoadOp::Clear;
  depth.stencilLoadOp=rhi::LoadOp::DontCare;depth.stencilStoreOp=rhi::StoreOp::DontCare;
  r.frame_fail_stage="gbuffer_pass_begin";
  rhi::RenderState state{};
  state.viewports[0]=rhi::Viewport::fromSize(static_cast<float>(render_width),static_cast<float>(render_height));state.viewportCount=1;
  state.scissorRects[0]=rhi::ScissorRect::fromSize(static_cast<std::uint32_t>(render_width),static_cast<std::uint32_t>(render_height));state.scissorRectCount=1;
  rhi::RenderPassColorAttachment colors[world_gbuffer_formats.size()]{};
  for(unsigned i=0;i<target.hdr.attachment_count;++i) {colors[i].view=target.hdr.views[i];colors[i].loadOp=rhi::LoadOp::Clear;colors[i].storeOp=rhi::StoreOp::Store;}
  rhi::RenderPassDesc pass{};pass.colorAttachments=colors;pass.colorAttachmentCount=1;pass.depthStencilAttachment=&depth;
  auto* render=commands->beginRenderPass(pass);if(!render) return trace.failed();
  render->setRenderState(state);
  r.frame_fail_stage="sky";
  bool success=render_sky(render,r.sky_pipeline,r.sky,camera.yaw,camera.pitch,camera.vertical_fov,render_width,render_height,camera.jitter_x,camera.jitter_y);
  render->end();if(!success) return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  trace.begin("map_gbuffer_encode");
  for(const auto& map:r.resident_maps)
    if(!prepare_map_draws(map.get(),commands.get(),camera,r))return trace.failed();
  colors[0].loadOp=rhi::LoadOp::Load;pass.colorAttachmentCount=target.hdr.attachment_count;
  render=commands->beginRenderPass(pass);if(!render) return trace.failed();
  render->setRenderState(state);r.frame_fail_stage="map_gbuffer";
  for(const auto& map:r.resident_maps)if(success)success=render_map(map.get(),render,camera,r,false);
  render->end();if(!success) return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  if(!r.items.instances.empty()) {
    r.lighting_profile.begin_pass(commands,LightingPass::DynamicGeometry);
    for(unsigned i=0;i<target.hdr.attachment_count;++i)colors[i].loadOp=rhi::LoadOp::Load;
    depth.depthLoadOp=rhi::LoadOp::Load;
    render=commands->beginRenderPass(pass);if(!render)return trace.failed();
    success=render_items(r,render,false);render->end();if(!success)return trace.failed();
    r.lighting_profile.mark(commands,LightingPass::DynamicGeometry);
  }
  // Two-phase Hi-Z occlusion (indirect maps only): rebuild the pyramid from
  // current depth, retest the phase-1 occluded set, draw the newly visible.
  bool map_occlusion=false;
  for(const auto& map:r.resident_maps)map_occlusion=map_occlusion || map_occlusion_active(map.get());
  if(map_occlusion) {
    r.frame_fail_stage="map_hiz";
    if(!build_world_hiz(r.hiz,commands.get(),target.depth.get()))return trace.failed();
    for(const auto& map:r.resident_maps)
      if(!prepare_map_draws(map.get(),commands.get(),camera,r,1))return trace.failed();
    r.frame_fail_stage="map_gbuffer_phase2";
    for(unsigned i=0;i<target.hdr.attachment_count;++i)colors[i].loadOp=rhi::LoadOp::Load;
    depth.depthLoadOp=rhi::LoadOp::Load;
    render=commands->beginRenderPass(pass);if(!render)return trace.failed();
    render->setRenderState(state);
    for(const auto& map:r.resident_maps)
      if(success && map_occlusion_active(map.get()))
        success=render_map(map.get(),render,camera,r,false,true);
    render->end();if(!success)return trace.failed();
  }
  const float sun[4]={-r.sky.light_direction_sky[0],-r.sky.light_direction_sky[1],-r.sky.light_direction_sky[2],r.lighting.sun_strength};
  trace.begin("lighting_encode");
  r.frame_fail_stage="lighting";
  if(!render_lighting(r,commands))return trace.failed();
  trace.begin("dynamic_receivers");
  r.frame_fail_stage="dynamic_receivers";
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  trace.begin("forward_encode");
  colors[0].view=target.hdr.scene_view;pass.colorAttachmentCount=1;depth.depthLoadOp=rhi::LoadOp::Load;
  if(r.temporal.mode) {
    auto& temporal=r.temporal.targets[r.active_frame];
    colors[1].view=temporal.object_view;colors[1].loadOp=rhi::LoadOp::Clear;
    pass.colorAttachmentCount=2;
  }
  if(r.temporal.mode) {
    render=commands->beginRenderPass(pass);if(!render)return trace.failed();
    render->end();
    if(!r.items.instances.empty()) {
      r.lighting_profile.begin_pass(commands,LightingPass::DynamicMotion);
      rhi::RenderPassColorAttachment motion_color{};
      motion_color.view=r.temporal.targets[r.active_frame].object_view;
      motion_color.loadOp=rhi::LoadOp::Load;motion_color.storeOp=rhi::StoreOp::Store;
      rhi::RenderPassDesc motion_pass{};motion_pass.colorAttachments=&motion_color;
      motion_pass.colorAttachmentCount=1;motion_pass.depthStencilAttachment=&depth;
      render=commands->beginRenderPass(motion_pass);
      if(!render)return trace.failed();
      success=render_items(r,render,true);render->end();if(!success)return trace.failed();
      r.lighting_profile.mark(commands,LightingPass::DynamicMotion);
    }
    // Reactive comparison includes all depth-writing opaque geometry.
    r.lighting_profile.begin_pass(commands,LightingPass::ReactiveCopy);
    commands->copyTexture(r.temporal.targets[r.active_frame].opaque,{0,1,0,1},{},target.hdr.scene,{0,1,0,1},{},
        {static_cast<unsigned>(render_width),static_cast<unsigned>(render_height),1});
    r.lighting_profile.mark(commands,LightingPass::ReactiveCopy);
    pass.colorAttachmentCount=1;
  }
  if(r.clouds) {
    r.lighting_profile.begin_pass(commands,LightingPass::Clouds);
    render=commands->beginRenderPass(pass);if(!render)return trace.failed();render->setRenderState(state);
    const float position[3]={camera.x,camera.y,camera.z};
    success=render_clouds(render,r.cloud_pipeline,r.sky,position,camera.yaw,camera.pitch,
        camera.vertical_fov,render_width,render_height,14000.f,.1f,8192,camera.jitter_x,camera.jitter_y,target.hdr.views[1]);
    render->end();if(!success)return trace.failed();
    r.lighting_profile.mark(commands,LightingPass::Clouds);
  }
  if(r.map) {
    r.frame_fail_stage="map_forward";
    r.lighting_profile.begin_pass(commands,LightingPass::MapForward);
    render=commands->beginRenderPass(pass);if(!render)return trace.failed();render->setRenderState(state);
    success=render_maps_forward(r.resident_maps,render,camera,r);
    render->end();if(!success)return trace.failed();
    r.lighting_profile.mark(commands,LightingPass::MapForward);
  }
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  trace.begin("temporal_encode");
  r.frame_fail_stage="temporal";
  if(!prepare_temporal(r.temporal,commands,r.active_frame,target.depth,target.hdr.scene_view,target.hdr.views[3]) ||
      !resolve_temporal(r.temporal,commands,r.active_frame,target.depth,target.hdr.scene))return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  auto* reconstructed=r.temporal.mode?r.temporal.targets[r.active_frame].output_view.get():nullptr;
  trace.begin("present_hdr_encode");
  r.frame_fail_stage="present_hdr";
  if(!present_world_hdr(commands,target.hdr,target.color_view,static_cast<unsigned>(r.width),static_cast<unsigned>(r.height),reconstructed)) return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  trace.begin("rml_encode");
  r.frame_fail_stage="rml";
  if(!render_rml(r.ui_renderer,commands,target.color_view,r.ui_context,r.width,r.height))return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  trace.begin("copy_encode");
  // Standalone RHI tracks all attachment, shader, copy and present transitions.
  // copyTexture requires concrete mip/layer counts; kEntireTexture is a state/view sentinel.
  const rhi::SubresourceRange copy_range{0,1,0,1};
  commands->copyTexture(image,copy_range,{},target.color,copy_range,{},
      {static_cast<std::uint32_t>(r.width),static_cast<std::uint32_t>(r.height),1});
  commands->setTextureState(image,rhi::ResourceState::Present);
  if(r.gpu_profile)r.gpu_profile->mark(commands.get());
  if(r.temporal.resolution.active)r.temporal.timing.end(commands,r.active_frame);
  trace.begin("command_finish");
  r.ray_diagnostics.end(commands);
  auto submission=commands->finish();
  if(!submission) return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("submit_guard");
  r.frame_fail_stage="submit";
  const auto watchdog=frame_watchdog_ms();
  const auto within_budget=[&]() {
    const auto frame_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
    if(!watchdog || frame_ms<=static_cast<double>(watchdog))return true;
    std::fprintf(stderr,"world_frame_watchdog ms=%.1f budget_ms=%llu status=%s\n",frame_ms,
        static_cast<unsigned long long>(watchdog),r.status.c_str());
    r.status="frame_watchdog";return trace.failed();
  };
  // A slow wait/compile must not enqueue another expensive frame before failing.
  if(!within_budget())return trace.failed();
  trace.begin("queue_submit");
  if(!r.frame_queue.submit(r.queue,submission,r.active_frame,r.frames))return trace.failed();
  submission_guard.submitted();
  if(r.gpu_counters)r.gpu_counters->submitted(r.frame_queue.fence(),r.frame_queue.last_signal());
  if(r.tile_session)r.tile_session->submitted(r.frame_queue.fence(),r.frame_queue.last_signal());
  trace.begin("frame_commit");
  commit_world_atlas(r.atlas);
  if(!within_budget())return trace.failed();
  r.lighting_profile.submit(r.frames);
  if(r.temporal.resolution.active)r.temporal.timing.submit(r.active_frame);
  commit_temporal(r.temporal);
  commit_map_reflections(r);
  target.initialized=true;
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("surface_present");
  if(!world_rhi_ok(r.surface->present())) return trace.failed();
  if(r.gpu_profile)r.gpu_profile->mark_cpu();
  trace.begin("serialized_frame_fence");
  if(r.frame_queue.count()==1) {
    const auto serialized_start=std::chrono::steady_clock::now();
    if(!r.frame_queue.wait(r.active_frame,frame_fence_timeout_ms(),trace.enabled()?&trace:nullptr))return trace.failed();
    if(r.gpu_profile)r.gpu_profile->add_wait(std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-serialized_start).count());
  }
  trace.begin("gpu_profile_finish");
  if(r.gpu_profile && !r.gpu_profile->finish(r.frames,r.width,r.height,r.frame_queue.count()))return trace.failed();
  trace.begin("serialized_gpu_query_resolve");
  if(r.frame_queue.count()==1 && r.gpu_profile && !r.gpu_profile->resolve(r.active_frame))return trace.failed();
  trace.begin("capture");
  if(r.debug.errors.load(std::memory_order_relaxed)!=0 || !world_renderer_capture(r,camera)) return trace.failed();
  trace.begin("renderer_local_cleanup");
  ++r.frames;return true;
}
}
