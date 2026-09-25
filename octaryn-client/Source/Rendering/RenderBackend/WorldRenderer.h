#pragma once
#include "WorldRetirementProgress.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include "LightingOptions.h"
#include "LocalLight.h"
struct lighting_settings;
namespace Rml { class RenderInterface; class Context; }
struct SDL_Window;
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldSceneSettings {
  double day_fraction{0.5}, seconds{};
  bool gradient{true}, stars{true}, sun{true}, moon{true}, pbr{true}, pom{true};
  bool clouds{true},fog{true};float fog_distance{1024};
  unsigned upscaler_mode{};
  bool fsr_sharpening{true};float fsr_sharpness{0.2f},fsr_render_scale{0.667f};
  bool fsr_dynamic_resolution{};float fsr_min_scale{0.5f},fsr_max_scale{1.f};
  unsigned fsr_target_fps{60};
  bool ray_tracing{true};
};
struct PlayerPose;
struct WorldCamera {
  float x{}, y{}, z{}, yaw{}, pitch{};
  float vertical_fov{1.04719755f}; // Radians; yaw zero faces negative Z.
  float jitter_x{},jitter_y{}; // Clip-space offset; zero for unjittered rendering.
};
struct WorldRendererStats {
  std::uint64_t gpu_bytes{}, frames{};
  bool map_ready{};
  std::uint32_t map_primitives{};
  unsigned upscaler_mode{},render_width{},render_height{},display_width{},display_height{};
  std::uint64_t temporal_resets{};
  bool fsr_dynamic_active{};float fsr_render_scale{1.f},fsr_gpu_ms{};
  bool ray_tracing_available{},ray_tracing_active{};
  bool gi_ready{};
};
// Initialization has exclusive RHI ownership. A host running it on a worker
// must synchronously dispatch window/surface operations to the main thread.
using WorldBootProgressFn = void (*)(const char* stage, void* user);
using WorldBootMainFn = void (*)(void (*operation)(void*), void* argument, void* user);
WorldRenderer* open_world_renderer_create(SDL_Window* window, WorldBootProgressFn progress, void* progress_user,
    WorldBootMainFn main_thread = nullptr);
void open_world_renderer_set_scene(WorldRenderer*, const WorldSceneSettings&);
void open_world_renderer_set_present(WorldRenderer*, int present_mode);
void open_world_renderer_set_player(WorldRenderer*,const PlayerPose&);
void open_world_renderer_set_capture_enabled(WorldRenderer*,bool enabled);
bool open_world_renderer_captured(const WorldRenderer*);
Rml::RenderInterface* open_world_renderer_ui_interface(WorldRenderer*);
void open_world_renderer_set_ui_context(WorldRenderer*,Rml::Context*);
void open_world_renderer_set_lighting(WorldRenderer*,const lighting_settings&);
bool open_world_renderer_set_lighting_options(WorldRenderer*,const LightingSettings&);
void open_world_renderer_set_lighting_debug(WorldRenderer*,unsigned debug_view);
void open_world_renderer_set_reflection_quality(WorldRenderer*,unsigned quality);
void open_world_renderer_set_shadow_quality(WorldRenderer*,unsigned quality);
void open_world_renderer_set_raster_shadows(WorldRenderer*,int enabled);
void open_world_renderer_set_trace_ranges(WorldRenderer*,float shadow_distance,float reflection_distance);
// Render the RmlUi document alone to an offscreen image, expanded to its full
// content size so panels stretching past the window are captured whole.
bool open_world_renderer_capture_ui(WorldRenderer*,const char* path);
bool open_world_renderer_render(WorldRenderer*, const WorldCamera& camera);
// Loading/menu present: clears to black and draws only the RmlUi document. No
// world, player, or sky work runs, so it never implies a loaded world.
bool open_world_renderer_render_menu(WorldRenderer*);
WorldRendererStats open_world_renderer_stats(const WorldRenderer*);
const char* open_world_renderer_status(const WorldRenderer*);
bool open_world_renderer_load_map(WorldRenderer*, const char* glb_path);
// Requires exclusive renderer ownership; creates saved temporal targets without
// accessing the window or surface, before the first interactive world frame.
bool open_world_renderer_prepare_temporal(WorldRenderer*);
bool open_world_renderer_map_ready(const WorldRenderer*);
void open_world_renderer_destroy(WorldRenderer*);
bool open_world_renderer_flush(WorldRenderer*);
// Exclusive final teardown on the graphics/window owner. No world rendering
// resumes after begin; keep the device, targets and surface until destroy.
bool open_world_renderer_begin_retirement(WorldRenderer*);
// Retire bounded secondary ownership entries. Returns the remaining count;
// batch_size=0 only queries the count.
std::uint64_t open_world_renderer_retire_step(WorldRenderer*,std::uint32_t batch_size=32,double budget_ms=2.0);
// Submit a closing frame (maintenance work when minimized) and await its fence.
bool open_world_renderer_retirement_frame(WorldRenderer*);
// Recheck after maintenance: retiring command buffers can release their last owners.
std::uint64_t open_world_renderer_retirement_remaining(WorldRenderer*);
WorldRetirementProgress open_world_renderer_retirement_progress(WorldRenderer*);
}
