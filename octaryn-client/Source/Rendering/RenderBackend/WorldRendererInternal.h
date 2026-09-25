#pragma once
#include "WorldRenderer.h"
#include "WorldGpuProfile.h"
#include "WorldFrames.h"
#include "FrameWatchdog.h"
#include "WorldTargets.h"
#include "WorldRayTracing.h"
#include "WorldRayLighting.h"
#include "WorldRayDebug.h"
#include "RendererCapabilities.h"
#include "SceneChanges.h"
#include "RTShadowSystem.h"
#include "LocalLightingSystem.h"
#include "LightingProfile.h"
#include "LightingOptions.h"
#include "WorldTemporal.h"
#include "SkyRenderer.h"
#include "WorldAtlas.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include "MapReflections.h"
#include "BlockTransportLookup.h"
#include "MapRenderer.h"
#include "CloudRenderer.h"
#include "RmlRenderer.h"
#include <slang-rhi.h>
#include <SDL3/SDL.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include <array>
#include <string>
#include <utility>
#include <atomic>
#include <cstdio>
namespace octaryn::client::rendering {
inline bool world_rhi_ok(SlangResult result) { return SLANG_SUCCEEDED(result); }
struct WorldRhiDebug final : rhi::IDebugCallback {
  std::atomic<std::uint32_t> errors{};
  void SLANG_MCALL handleMessage(rhi::DebugMessageType type,rhi::DebugMessageSource,const char* message) noexcept override {
    if(type==rhi::DebugMessageType::Error) errors.fetch_add(1,std::memory_order_relaxed);
    const char* severity=type==rhi::DebugMessageType::Error?"error":type==rhi::DebugMessageType::Warning?"warning":"info";
    std::fprintf(stderr,"rhi_validation severity=%s %s\n",severity,message?message:"");
  }
};
// Device creation may reject an optional descriptor capacity before any usable
// device exists. Keep that attempt separate from validation of the accepted one.
struct WorldDeviceAttemptDebug final : rhi::IDebugCallback {
  WorldRhiDebug* destination;
  std::mutex mutex;
  bool accepted{};
  std::uint32_t errors{};
  explicit WorldDeviceAttemptDebug(WorldRhiDebug* target):destination(target) {}
  void accept() {std::lock_guard lock(mutex);accepted=true;destination->errors.fetch_add(errors);}
  void SLANG_MCALL handleMessage(rhi::DebugMessageType type,rhi::DebugMessageSource source,const char* message) noexcept override {
    std::lock_guard lock(mutex);
    if(accepted) {destination->handleMessage(type,source,message);return;}
    if(type==rhi::DebugMessageType::Error)++errors;
    const char* severity=type==rhi::DebugMessageType::Error?"error":type==rhi::DebugMessageType::Warning?"warning":"info";
    std::fprintf(stderr,"rhi_device_attempt severity=%s %s\n",severity,message?message:"");
  }
};
struct WorldRenderer {
  SkyUniforms sky{};
  SkyLighting lighting{};
  bool pbr{true},pom{true},clouds{true},ray_requested{true},ray_enabled{true},ray_effects{true};float fog_distance{1024};
  lighting_settings lighting_config{lighting_settings_default_value()};
  SDL_Window* window{};
  WorldRhiDebug debug;
  WorldDeviceAttemptDebug device_attempt{&debug};
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> queue;
  WorldFrames frame_queue;
  std::unique_ptr<WorldGpuProfile> gpu_profile;
  std::unique_ptr<WorldRayTracing> ray_tracing;
  RendererCapabilities capabilities;
  SceneChanges scene_changes;
  LightingSettings lighting_settings;
  RTShadowSystem rt_shadows;
  WorldRayDebug ray_debug;
  LocalLightingSystem local_lighting;
  LightingProfile lighting_profile;
  Slang::ComPtr<rhi::ISurface> surface;
  Slang::ComPtr<rhi::IRenderPipeline> sky_pipeline,cloud_pipeline;
  std::array<WorldTargets,2> targets;
  WorldTemporal temporal;
  MapReflections map_reflections;
  BlockTransportLookup block_transport_lookup;
  int render_width() const {return temporal.mode?static_cast<int>(temporal.width):width;}
  int render_height() const {return temporal.mode?static_cast<int>(temporal.height):height;}
  unsigned active_frame{};
  WorldTargets& target() {return targets[active_frame];}
  MapRenderer* map{};
  RmlRenderer* ui_renderer{};Rml::Context* ui_context{};
  WorldAtlas* atlas{};
  rhi::Format color_format{rhi::Format::RGBA8Unorm};
  bool captured{},capture_enabled{true};
  bool boot_captured{};
  std::uint64_t capture_scene_revision{},capture_stable_frame{};
  unsigned capture_count{};
  std::uint64_t capture_last_frame{};
  int width{},height{};
  float camera_position[3]{};
  std::array<float,20> view_uniforms{}; // Eye, view-projection with temporal jitter, projection scalars.
  int present_mode{};
  bool present_dirty{true};
  std::uint64_t frames{};
  bool retirement_started{};
  bool culling_enabled{true};
  std::string status{"initializing"};
  const char* frame_fail_stage{"none"};
  ~WorldRenderer() {
    if(queue && !frame_queue.synchronize(queue,frame_fence_timeout_ms()))
      frame_gpu_shutdown_failed("renderer_queue");
    if(gpu_profile && !gpu_profile->drain())
      std::fputs("World frame profiling drain failed\n",stderr);
    lighting_profile.drain();
    destroy_rml_renderer(ui_renderer);
    destroy_map_renderer(map);destroy_world_atlas(atlas);
  }
};
bool world_renderer_create_device(WorldRenderer&, WorldBootProgressFn progress, void* progress_user, WorldBootMainFn main_thread);
bool world_renderer_boot_frame(WorldRenderer&, const char* stage);
bool world_renderer_resize(WorldRenderer&,int width,int height);
bool world_renderer_capture(WorldRenderer&,const WorldCamera&);
}
