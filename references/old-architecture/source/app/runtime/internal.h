#pragma once

#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "app/audio/audio.h"
#include "app/display/menu.h"
#include "app/runtime/frame_profile.h"
#include "app/runtime/frame_pacing.h"
#include "app/runtime/options.h"
#include "app/lighting/settings/settings.h"
#include "render/pipelines/pipelines.h"
#include "render/scene/passes.h"
#include "render/resources/resources.h"
#include "app/runtime/settings.h"
#include "render/ui/ui.h"
#include "app/window/window.h"
#include "app/player/player.h"
#include "core/persistence/persistence.h"
#include "core/world_time/time.h"
#include "world/runtime/world.h"

extern const char* APP_NAME;
extern const int PLAYER_ID;
extern const int DISTANCE_OPTIONS[];
extern const int DISTANCE_OPTION_COUNT;
extern int WORKER_OPTIONS[OCTARYN_WORLD_JOBS_MAX_WORKERS];
extern int WORKER_OPTION_COUNT;

extern SDL_Window* window;
extern SDL_GPUDevice* device;
extern SDL_GPUTextureFormat color_format;
extern SDL_GPUTextureFormat depth_format;
extern main_pipelines_t pipelines;
extern main_render_resources_t resources;
extern player_t player;
extern Uint64 ticks1;
extern Uint64 ticks2;
extern bool show_debug_overlay;
extern bool fog_enabled;
extern bool clouds_enabled;
extern bool sky_gradient_enabled;
extern bool stars_enabled;
extern bool sun_enabled;
extern bool moon_enabled;
extern bool pom_enabled;
extern bool pbr_enabled;
extern int startup_render_distance;
extern int startup_target_render_distance;
extern int startup_bootstrap_render_distance;
extern bool startup_render_distance_bootstrap_pending;
extern int worldgen_worker_count;
extern main_display_menu_t display_menu;
extern main_runtime_settings_t runtime_settings;
extern app_runtime_options_t runtime_options;
extern main_window_t main_window;
extern main_frame_profile_t frame_profile;
extern app_frame_pacing_t frame_pacing;
extern bool first_frame_logged;
extern bool gpu_validation_enabled;
extern bool gpu_validation_unavailable;
extern bool detached_launch_mode;
extern bool player_loaded_from_save;
extern bool world_loaded_from_save;
extern bool spawn_surface_aligned;
extern bool spawn_fallback_used;
extern bool terrain_debug_logged;
extern int terrain_debug_frame_count;
extern bool restore_relative_mouse_after_ui;
extern bool lighting_tuning_dirty;
extern Uint64 last_profile_spike_log_ticks;
extern Uint64 lighting_tuning_change_ticks;
extern bool quit_requested;
extern Uint64 startup_ticks_ns;
extern bool window_claimed_for_gpu;
extern bool audio_initialized;
extern bool imgui_initialized;
extern bool resources_initialized;
extern bool pipelines_initialized;
extern bool persistence_initialized;
extern bool world_initialized;
extern world_time_state_t world_clock;
extern world_time_snapshot_t world_clock_snapshot;
extern main_lighting_tuning_t lighting_tuning;
