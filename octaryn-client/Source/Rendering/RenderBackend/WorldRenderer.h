#pragma once
#include <cstdint>
#include <memory>
#include "LightingQuality.h"
#include "LocalLight.h"
struct lighting_settings;
namespace Rml { class RenderInterface; class Context; }
struct SDL_Window;
namespace octaryn::client::world_presentation { class WorldStream;struct StreamColumn;struct WorldItemSnapshot; }
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
struct SelectionTarget;
struct WorldCamera {
  float x{}, y{}, z{}, yaw{}, pitch{};
  float vertical_fov{1.04719755f}; // Radians; yaw zero faces negative Z.
  float jitter_x{},jitter_y{}; // Clip-space offset; zero for unjittered rendering.
};
struct WorldRendererStats {
  std::uint32_t columns{};
  std::uint64_t quads{}, gpu_bytes{}, frames{};
  std::uint32_t drawn_columns{};
  std::uint64_t drawn_quads{};
  std::uint32_t pending_meshes{};
  unsigned upscaler_mode{},render_width{},render_height{},display_width{},display_height{};
  std::uint64_t temporal_resets{};
  bool fsr_dynamic_active{};float fsr_render_scale{1.f},fsr_gpu_ms{};
  bool ray_tracing_available{},ray_tracing_active{};
  std::uint32_t ray_ready_columns{},ray_pending_columns{};
};
// Initialization has exclusive RHI ownership. A host running it on a worker
// must synchronously dispatch window/surface operations to the main thread.
using WorldBootProgressFn = void (*)(const char* stage, void* user);
using WorldBootMainFn = void (*)(void (*operation)(void*), void* argument, void* user);
WorldRenderer* open_world_renderer_create(SDL_Window* window, WorldBootProgressFn progress, void* progress_user,
    WorldBootMainFn main_thread = nullptr);
void open_world_renderer_set_scene(WorldRenderer*, const WorldSceneSettings&);
void open_world_renderer_set_present(WorldRenderer*, int present_mode);
void open_world_renderer_set_selection(WorldRenderer*,const SelectionTarget&);
void open_world_renderer_set_player(WorldRenderer*,const PlayerPose&);
void open_world_renderer_set_items(WorldRenderer*,std::shared_ptr<const world_presentation::WorldItemSnapshot>);
void open_world_renderer_set_capture_enabled(WorldRenderer*,bool enabled);
bool open_world_renderer_captured(const WorldRenderer*);
Rml::RenderInterface* open_world_renderer_ui_interface(WorldRenderer*);
void open_world_renderer_set_ui_context(WorldRenderer*,Rml::Context*);
unsigned open_world_renderer_ui_tile(WorldRenderer*,std::uint16_t selected_block);
void open_world_renderer_set_lighting(WorldRenderer*,const lighting_settings&);
bool open_world_renderer_set_lighting_options(WorldRenderer*,const LightingSettings&);
void open_world_renderer_set_lighting_debug(WorldRenderer*,unsigned debug_view);
void open_world_renderer_set_lighting_quality(WorldRenderer*,unsigned quality);
void open_world_renderer_set_raster_shadows(WorldRenderer*,int enabled);
void open_world_renderer_set_trace_ranges(WorldRenderer*,float shadow_distance,float reflection_distance);
bool open_world_renderer_set_ddgi_range(WorldRenderer*,unsigned voxel_radius,unsigned coarse_radius);
// Render the RmlUi document alone to an offscreen image, expanded to its full
// content size so panels stretching past the window are captured whole.
bool open_world_renderer_capture_ui(WorldRenderer*,const char* path);
// Advance one bounded delivery without blocking; publish before camera queries.
bool open_world_renderer_stream(WorldRenderer*,world_presentation::WorldStream&);
// Synchronous replacement for explicit mesh qualification.
bool open_world_renderer_update(WorldRenderer*,
    const world_presentation::StreamColumn& column);
// Optimistic local block edit: lights react this frame, remesh follows from the
// mutated source. The authoritative snapshot confirms or heals it. Returns false
// when the column is not resident.
bool open_world_renderer_can_predict(const WorldRenderer*);
bool open_world_renderer_apply_predicted_edit(WorldRenderer*,std::uint64_t command,std::int32_t x,std::int32_t y,std::int32_t z,std::uint16_t block);
void open_world_renderer_resolve_predicted_edit(WorldRenderer*,std::uint64_t command,bool accepted,std::uint64_t revision);
void open_world_renderer_reset_predictions(WorldRenderer*);
bool open_world_renderer_render(WorldRenderer*, const WorldCamera& camera);
// Main-menu present: clears to black and draws only the RmlUi document. No
// world, player, or sky work runs, so the menu never implies a loaded world.
bool open_world_renderer_render_menu(WorldRenderer*);
void open_world_renderer_set_center(WorldRenderer*, std::int32_t x,
                                    std::int32_t z, int radius);
WorldRendererStats open_world_renderer_stats(const WorldRenderer*);
const char* open_world_renderer_status(const WorldRenderer*);
void open_world_renderer_destroy(WorldRenderer*);
bool open_world_renderer_flush(WorldRenderer*);
}
