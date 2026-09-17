#pragma once
#include "WorldRenderer.h"
#include "PredictedBlocks.h"
#include "WorldGpuProfile.h"
#include "WorldBatch.h"
#include "WorldHaloJobs.h"
#include "WorldDeliveryJobs.h"
#include "WorldMeshTimings.h"
#include "WorldFrames.h"
#include "WorldTargets.h"
#include "WorldRayTracing.h"
#include "WorldRayLighting.h"
#include "WorldRayDebug.h"
#include "RendererCapabilities.h"
#include "SceneChanges.h"
#include "RTShadowSystem.h"
#include "ShadowFallbackSystem.h"
#include "LocalShadowSystem.h"
#include "DDGISystem.h"
#include "LocalLightingSystem.h"
#include "BlockLights.h"
#include "LightingProfile.h"
#include "LightingQuality.h"
#include "WorldTemporal.h"
#include "SkyRenderer.h"
#include "WorldAtlas.h"
#include "RhiShader.h"
#include "WorldHdr.h"
#include "CloudRenderer.h"
#include "PlayerRenderer.h"
#include "WorldItemsRenderer.h"
#include "RmlRenderer.h"
#include "SelectionRenderer.h"
#include <slang-rhi.h>
#include <SDL3/SDL.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include "WorldStream.h"
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
struct WorldColumnGpu {
  Slang::ComPtr<rhi::IBuffer> faces,arguments,fluids,patches;
  std::uint64_t batch_faces{},batch_patches{};
  bool batch_handles_ready{};
  std::uint32_t face_count{};
  std::array<std::uint32_t,5> pass_counts{};
  std::array<std::uint32_t,5> patch_counts{};
  int min_y{},height{};
};
struct WorldVisibleColumn { WorldColumnGpu* column;float distance; };
struct WorldDrawList {
  std::vector<WorldVisibleColumn> visible;
  std::uint64_t quads{};
};
struct WorldRenderer {
  SkyUniforms sky{};
  SkyLighting lighting{};
  bool pbr{true},pom{true},clouds{true},ray_enabled{true};float fog_distance{256};
  lighting_settings lighting_config{lighting_settings_default_value()};
  SDL_Window* window{};
  WorldMeshTimings mesh_timings;
  WorldRhiDebug debug;
  WorldDeviceAttemptDebug device_attempt{&debug};
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> queue;
  WorldFrames frame_queue;
  std::unique_ptr<WorldGpuProfile> gpu_profile;
  std::unique_ptr<WorldBatch> batch;
  std::unique_ptr<WorldHaloJobs> halo_jobs;
  std::unique_ptr<WorldMeshJob> qualification_mesh;
  std::unique_ptr<WorldDeliveryJobs> delivery_jobs;
  std::unique_ptr<WorldRayTracing> ray_tracing;
  RendererCapabilities capabilities;
  SceneChanges scene_changes;
  LightingSettings lighting_settings;
  RTShadowSystem rt_shadows;
  WorldRayDebug ray_debug;
  ShadowFallbackSystem shadow_fallback;
  LocalShadowSystem local_shadows;
  DDGISystem ddgi;
  LocalLightingSystem local_lighting;
  BlockLights block_lights;
  LightingProfile lighting_profile;
  Slang::ComPtr<rhi::IRenderPipeline> ray_water_pipeline;
  Slang::ComPtr<rhi::ISurface> surface;
  Slang::ComPtr<rhi::IComputePipeline> mesh_pipeline;
  Slang::ComPtr<rhi::IRenderPipeline> raster_pipeline,sprite_pipeline,transparent_pipeline,lava_pipeline,sky_pipeline,cloud_pipeline,selection_pipeline;
  SelectionTarget selection;
  std::array<WorldTargets,2> targets;
  WorldTemporal temporal;
  int render_width() const {return temporal.mode?static_cast<int>(temporal.width):width;}
  int render_height() const {return temporal.mode?static_cast<int>(temporal.height):height;}
  unsigned active_frame{};
  WorldTargets& target() {return targets[active_frame];}
  PlayerRenderer* player{};PlayerPose player_pose{};
  WorldItemsRenderer* items{};
  std::chrono::steady_clock::time_point item_frame_time{};
  std::shared_ptr<const world_presentation::WorldItemSnapshot> item_snapshot;
  RmlRenderer* ui_renderer{};Rml::Context* ui_context{};
  WorldAtlas* atlas{};
  rhi::Format color_format{rhi::Format::RGBA8Unorm};
  bool captured{},capture_enabled{true};
  std::uint64_t capture_scene_revision{},capture_stable_frame{};
  unsigned capture_count{};
  std::uint64_t capture_last_frame{};
  std::map<std::pair<std::int32_t,std::int32_t>,WorldColumnGpu> columns;
  // Rebuilt after column mutations, then shared by both draws in this frame.
  WorldDrawList draw_list;
  std::array<float,36> draw_uniforms{};
  std::map<std::pair<std::int32_t,std::int32_t>,world_presentation::StreamColumn> sources;
  std::set<std::pair<std::int32_t,std::int32_t>> dirty;
  // Existing-source boundary edits outrank initial residency halo rebuilds.
  std::set<std::pair<std::int32_t,std::int32_t>> dirty_urgent;
 world_presentation::PredictedBlocks predicted_edits;
 std::map<std::pair<std::int32_t,std::int32_t>,world_presentation::StreamColumn> prediction_bases;
  int width{},height{},center_x{},center_z{},radius{4};
  int present_mode{};
  bool present_dirty{true};
  std::uint64_t frames{};
  std::uint32_t drawn_columns{};
  std::uint64_t drawn_quads{};
  std::uint64_t resident_quads{},column_gpu_bytes{};
  bool culling_enabled{true};
  std::string status{"initializing"};
  ~WorldRenderer() {
    if(!frame_queue.drain() || (gpu_profile && !gpu_profile->drain()))
      std::fputs("World frame profiling drain failed\n",stderr);
    if(queue)queue->waitOnHost();
    lighting_profile.drain();
    destroy_rml_renderer(ui_renderer);destroy_player_renderer(player);destroy_world_items_renderer(items);destroy_world_atlas(atlas);
  }
};
bool world_renderer_create_device(WorldRenderer&, WorldBootProgressFn progress, void* progress_user);
bool world_renderer_resize(WorldRenderer&,int width,int height);
bool world_renderer_mesh(WorldRenderer&,const world_presentation::StreamColumn&,WorldColumnGpu&);
// Advance GPU phases only; query/source/visible publication remains pre-camera.
bool world_renderer_progress_delivery(WorldRenderer&);
void world_prepare_draw_list(WorldDrawList&,
  std::map<std::pair<std::int32_t,std::int32_t>,WorldColumnGpu>&,
  const WorldCamera&,int width,int height,bool culling_enabled);
void world_renderer_prepare_draw(WorldRenderer&,const WorldCamera&);
const WorldVisibleColumn& world_draw_item(const WorldDrawList&,std::size_t index,bool forward);
void world_renderer_store_column(WorldRenderer&,std::pair<std::int32_t,std::int32_t>,WorldColumnGpu);
bool world_renderer_draw(WorldRenderer&,rhi::IRenderPassEncoder*,bool forward);
std::vector<std::uint32_t> world_mesh_halo(const WorldRenderer&,const world_presentation::StreamColumn&);
bool world_mesh_refresh_one(WorldRenderer&);
bool world_mesh_take_pending(WorldRenderer&,std::pair<std::int32_t,std::int32_t>&);
// Call before replacing the retained source; preserve pending neighbor work.
void world_mesh_invalidate_neighbors(WorldRenderer&,const world_presentation::StreamColumn&);
// Rebase pending commands; retire accepted commands only at their receipt revision.
void world_renderer_reapply_predicted_edits(WorldRenderer&,const std::pair<std::int32_t,std::int32_t>&);
bool world_renderer_same_authoritative_content(const WorldRenderer&,const world_presentation::StreamColumn&);
void world_renderer_publish_column_metadata(WorldRenderer&,const world_presentation::StreamColumn&);
bool world_renderer_capture(WorldRenderer&,const WorldCamera&);
}
