#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/runtime/internal.h"
#include "app/runtime/lifecycle.h"
#include "core/render_distance.h"

const char* APP_NAME = "Octaryn Engine";
extern const int PLAYER_ID = 0;
extern const int DISTANCE_OPTIONS[] = {4, 8, 12, 16, 20, 24, 32};
extern const int DISTANCE_OPTION_COUNT = OCTARYN_RENDER_DISTANCE_OPTION_COUNT;
int WORKER_OPTIONS[OCTARYN_WORLD_JOBS_MAX_WORKERS] = {};
int WORKER_OPTION_COUNT = 0;

SDL_Window* window = nullptr;
SDL_GPUDevice* device = nullptr;
SDL_GPUTextureFormat color_format = SDL_GPU_TEXTUREFORMAT_INVALID;
SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
main_pipelines_t pipelines = {};
main_render_resources_t resources = {};
player_t player = {};
Uint64 ticks1 = 0;
Uint64 ticks2 = 0;
bool show_debug_overlay = true;
bool fog_enabled = true;
bool clouds_enabled = true;
bool sky_gradient_enabled = true;
bool stars_enabled = true;
bool sun_enabled = true;
bool moon_enabled = true;
bool pom_enabled = true;
bool pbr_enabled = true;
int startup_render_distance = 16;
int startup_target_render_distance = 16;
int startup_bootstrap_render_distance = 16;
bool startup_render_distance_bootstrap_pending = false;
int worldgen_worker_count = 0;
main_display_menu_t display_menu = {};
main_runtime_settings_t runtime_settings = {};
app_runtime_options_t runtime_options = {};
main_window_t main_window = {};
main_frame_profile_t frame_profile = {};
app_frame_pacing_t frame_pacing = {};
bool first_frame_logged = false;
bool gpu_validation_enabled = false;
bool gpu_validation_unavailable = false;
bool detached_launch_mode = false;
bool player_loaded_from_save = false;
bool world_loaded_from_save = false;
bool spawn_surface_aligned = false;
bool spawn_fallback_used = false;
bool terrain_debug_logged = false;
int terrain_debug_frame_count = 0;
bool restore_relative_mouse_after_ui = false;
bool lighting_tuning_dirty = false;
Uint64 last_profile_spike_log_ticks = 0;
Uint64 lighting_tuning_change_ticks = 0;
bool quit_requested = false;
Uint64 startup_ticks_ns = 0;
bool window_claimed_for_gpu = false;
bool audio_initialized = false;
bool imgui_initialized = false;
bool resources_initialized = false;
bool pipelines_initialized = false;
bool persistence_initialized = false;
bool world_initialized = false;
world_time_state_t world_clock = {};
world_time_snapshot_t world_clock_snapshot = {};
main_lighting_tuning_t lighting_tuning = {
    .visible = false,
    .fog_enabled = 1,
    .fog_distance = 256.0f,
    .skylight_floor = 0.08f,
    .ambient_strength = 0.82f,
    .sun_strength = 1.0f,
    .sun_fallback_strength = 1.0f,
};

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char** argv)
{
    return app_runtime_init(appstate, argc, argv);
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    app_runtime_quit(appstate, result);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    return app_runtime_frame(appstate);
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)
{
    return app_runtime_handle_event(appstate, event);
}
