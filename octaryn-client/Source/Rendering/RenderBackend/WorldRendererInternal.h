#pragma once
#include "WorldRenderer.h"
#include "../Performance/PerformanceProfile.h"
#include "../Items/ItemRenderer.h"
#include "WorldGpuProfile.h"
#include "GpuCounterProfile.h"
#include "FrameCpuProfile.h"
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
#include "RayDiagnosticProfile.h"
#include "ReflectionWaveMode.h"
#include "LightingOptions.h"
#include "WorldTemporal.h"
#include "WorldHiz.h"
#include "MapCullSet.h"
#include "SkyRenderer.h"
#include "WorldAtlas.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include "MapReflections.h"
#include "BlockTransportLookup.h"
#include "MapRenderer.h"
#include "TileSession.h"
#include "../../VirtualGeometry/WorldGeometry.h"
#include "../../VirtualGeometry/WorldGeometryRaster.h"
#include "../../VirtualGeometry/SceneSession.h"
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
namespace virtual_geometry {class SceneMemoryLedger;}
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
  PerformanceProfile performance_profile{requested_performance_profile()};
  SkyUniforms sky{};
  SkyLighting lighting{};
  MapSceneEnvironment scene_environment{};
  bool pbr{true},pom{true},clouds{true},ray_requested{true},ray_enabled{true},ray_effects{true};float fog_distance{1024};
  lighting_settings lighting_config{lighting_settings_default_value()};
  SDL_Window* window{};
  bool hidden_offscreen{};
  WorldRhiDebug debug;
  WorldDeviceAttemptDebug device_attempt{&debug};
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> queue;
  ItemRenderer items;
  WorldFrames frame_queue;
  std::unique_ptr<WorldGpuProfile> gpu_profile;
  std::unique_ptr<GpuCounterProfile> gpu_counters;
  FrameCpuProfile frame_cpu;
  std::unique_ptr<WorldRayTracing> ray_tracing;
  RendererCapabilities capabilities;
  SceneChanges scene_changes;
  LightingSettings lighting_settings;
  RTShadowSystem rt_shadows;
  WorldRayDebug ray_debug;
  LocalLightingSystem local_lighting;
  LightingProfile lighting_profile;
  RayDiagnosticProfile ray_diagnostics;
  ReflectionWaveMode reflection_wave;
  Slang::ComPtr<rhi::ISurface> surface;
  Slang::ComPtr<rhi::IRenderPipeline> sky_pipeline,cloud_pipeline;
  std::array<WorldTargets,2> targets;
  WorldTemporal temporal;
  WorldHiz hiz;
  MapCullSet map_cull;
  MapReflections map_reflections;
  BlockTransportLookup block_transport_lookup;
  int render_width() const {return temporal.mode?static_cast<int>(temporal.width):width;}
  int render_height() const {return temporal.mode?static_cast<int>(temporal.height):height;}
  unsigned active_frame{};
  WorldTargets& target() {return targets[active_frame];}
  MapRenderer* map{};
  std::vector<std::shared_ptr<MapRenderer>> resident_maps;
  // Independent owners publish into their own lists before the render union.
  std::vector<std::shared_ptr<MapRenderer>> tile_resident_maps,scene_resident_maps;
  std::string scene_physics_source;
  std::vector<MapForwardDraw> map_forward_order;
  std::uint64_t resident_texture_bytes{};
  std::unique_ptr<TileSession> tile_session;
  std::unique_ptr<SceneSession> scene_session;
  std::shared_ptr<virtual_geometry::SceneMemoryLedger> scene_memory;
  std::unique_ptr<virtual_geometry::WorldGeometryRaster> geometry_raster;
  WorldCamera tile_anchor;
  bool tile_anchor_valid{},tile_anchor_authoritative{};
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
  bool frame_failed{};
  bool culling_enabled{true};
  std::string status{"initializing"};
  WorldLoadProgressFn load_progress{};
  void* load_progress_user{};
  const char* frame_fail_stage{"none"};
  ~WorldRenderer() {
    if(queue && !frame_queue.synchronize(queue,frame_fence_timeout_ms()))
      frame_gpu_shutdown_failed("renderer_queue");
    if(gpu_counters)gpu_counters->shutdown();
    if(gpu_profile && !gpu_profile->drain())
      std::fputs("profile_writer_failed capture_invalid=1 owner=gpu_drain\n",stderr);
    if(gpu_profile && !gpu_profile->close())std::fputs("profile_writer_failed capture_invalid=1 owner=gpu_close\n",stderr);
    if(!lighting_profile.drain())std::fputs("profile_writer_failed capture_invalid=1 owner=lighting_drain\n",stderr);
    if(!lighting_profile.close())std::fputs("profile_writer_failed capture_invalid=1 owner=lighting_close\n",stderr);
    if(!ray_diagnostics.drain(device))std::fputs("profile_writer_failed capture_invalid=1 owner=ray_drain\n",stderr);
    if(!ray_diagnostics.close())std::fputs("profile_writer_failed capture_invalid=1 owner=ray_close\n",stderr);
    if(!frame_cpu.close())std::fputs("profile_writer_failed capture_invalid=1 owner=frame_cpu_shutdown\n",stderr);
    destroy_rml_renderer(ui_renderer);
    scene_session.reset();tile_session.reset();resident_maps.clear();tile_resident_maps.clear();scene_resident_maps.clear();geometry_raster.reset();map=nullptr;destroy_world_atlas(atlas);
  }
};
bool world_renderer_create_device(WorldRenderer&, WorldBootProgressFn progress, void* progress_user, WorldBootMainFn main_thread);
bool world_renderer_boot_frame(WorldRenderer&, const char* stage);
void world_renderer_load_stage(WorldRenderer&,const char* stage,bool cpu_only=false);
bool world_renderer_resize(WorldRenderer&,int width,int height);
bool world_renderer_capture(WorldRenderer&,const WorldCamera&);
void refresh_resident_texture_bytes(WorldRenderer&);
void publish_resident_maps(WorldRenderer&);
bool load_scene_physics_overlay(WorldRenderer&,const std::filesystem::path& manifest);
bool load_scene_physics_map(WorldRenderer&,const std::filesystem::path& source,bool& handled);
}
