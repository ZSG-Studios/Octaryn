#include "MapWorldSession.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "Camera.h"
#include "MapPlayer.h"
#include "LightingState.h"
#include "LoadingScreen.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "LightingDebugViews.h"
#include "UiData.h"
#include "DebugOverlay.h"
#include "PlayerView.h"
#include "FramePacing.h"
#include "FramePacingDisplay.h"
#include "FramePacingReport.h"
#include "FrameTimingLog.h"
#include "MapMotionValidation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace octaryn::client::app {
namespace graphics = octaryn::client::rendering;

// Mesh-map session: renders a Blender-exported GLB world with a locally
// simulated free-fly player. No server, voxel streaming, block edits,
// selection, inventory or streamed world items exist in this mode.
SessionOutcome run_map_world_session(MapSessionContext& ctx) {
  SDL_Window* window = ctx.window;
  const WorldRunOptions& options = *ctx.options;
  WorldProfile& profile = *ctx.profile;
  LightingState& lighting = *ctx.lighting;
  WorldControls& controls = *ctx.controls;
  MapPlayer& player = *ctx.player;
  int& width = *ctx.width;
  int& height = *ctx.height;
  auto* renderer = ctx.renderer;
  DebugOverlay* overlay = ctx.overlay;
  bool map_loading = true;
  ::camera camera_settings{};
  camera_init(&camera_settings, CAMERA_PROJECTION_PERSPECTIVE);
  unsigned frames = 0;
  int result = 0;
  FramePacing pacing;
  FrameTimingLog timing_log(SDL_getenv("OCTARYN_CLIENT_FRAME_TIMING_PATH"));
  MapMotionValidation camera_motion(options.benchmark_hidden,options.benchmark_seconds,
      options.frame_limit,SDL_getenv("OCTARYN_CLIENT_MAP_CAMERA_MOTION"),
      SDL_getenv("OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH"));
  const char* uncapped_env=SDL_getenv("OCTARYN_CLIENT_BENCHMARK_UNCAPPED");
  const bool benchmark_uncapped=options.benchmark_hidden && uncapped_env && *uncapped_env=='1';
  FramePacingReport pacing_report(options.validate_frame_pacing ? SDL_getenv("OCTARYN_CLIENT_PACING_PROFILE_PATH") : nullptr);
  const bool uncapped = benchmark_uncapped || (!options.benchmark_hidden && !options.validate_frame_pacing &&
      (options.frame_limit > 0 || options.benchmark_seconds > 0));
  if (!uncapped) pacing.update_display([&] { return frame_pacing_refresh_rate(window); });
  auto last = SDL_GetTicksNS();
  auto last_complete = last;
  const auto start = last;
  std::printf("open_world_start mode=map shader=slang backend=slang_rhi authority=local_player\n");
  std::fflush(stdout);
  while (controls.running) {
    const auto now = SDL_GetTicksNS();
    const double elapsed = static_cast<double>(now - last) / 1e9;
    last = now;
    frame_profile_sample sample{};
    sample.total_ms = static_cast<float>(elapsed * 1000.0);
    const auto event_start = SDL_GetTicksNS();
    read_world_controls(window, controls, !options.benchmark_hidden && options.benchmark_seconds <= 0);
    if (controls.display_changed) pacing.invalidate_display();
    if (!uncapped) pacing.update_display([&] { return frame_pacing_refresh_rate(window); });
    sample.misc_ms = frame_profile_elapsed_ms_since(event_start);
    if (!controls.running) break;
    const auto sim_start = SDL_GetTicksNS();
    map_player_update(player, controls.movement, controls.flying, controls.yaw, controls.pitch, elapsed);
    if (controls.time_hour_steps) map_player_step_hours(player, controls.time_hour_steps);
    sample.sim_ms = frame_profile_elapsed_ms_since(sim_start);
    const float zoom = static_cast<float>(1u << controls.zoom);
    const float fov = 2 * std::atan(std::tan(camera_settings.vertical_field_of_view_radians / 2) / zoom);
    auto camera = player_camera_map(player, controls, fov);
    camera_motion.apply(camera,frames);
    auto ui = graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(ui, profile.snapshot());
    SDL_GetWindowSizeInPixels(window, &width, &height);
    const auto ui_start = SDL_GetTicksNS();
    const auto resolution_stats = graphics::open_world_renderer_stats(renderer);
    overlay->update(ui, width, height);
    overlay->set_stat("render-res", std::to_string(resolution_stats.render_width) + "x" +
        std::to_string(resolution_stats.render_height));
    overlay->set_stat("rt", resolution_stats.ray_tracing_available ? "on" : "off");
    overlay->set_stat("scene", "map");
    sample.ui_ms = frame_profile_elapsed_ms_since(ui_start);
    graphics::open_world_renderer_set_lighting(renderer, lighting.values);
    graphics::open_world_renderer_set_lighting_debug(renderer, graphics::sanitize_lighting_debug(lighting.debug_view));
    graphics::open_world_renderer_set_reflection_quality(renderer, controls.ui.reflection_quality);
    graphics::open_world_renderer_set_shadow_quality(renderer, controls.ui.shadow_quality);
    graphics::open_world_renderer_set_raster_shadows(renderer, controls.ui.raster_sun_shadows);
    graphics::open_world_renderer_set_trace_ranges(renderer, float(controls.ui.shadow_distance),
        float(controls.ui.reflection_distance));
    graphics::open_world_renderer_set_present(renderer, options.benchmark_hidden ? 0 : controls.ui.present_mode_index);
    const auto& settings = controls.ui;
    graphics::open_world_renderer_set_scene(renderer,
        {player.world_day_fraction, player.source_seconds, settings.sky_gradient_enabled != 0,
         settings.stars_enabled != 0, settings.sun_enabled != 0, settings.moon_enabled != 0,
         settings.pbr_enabled != 0, settings.pom_enabled != 0, settings.clouds_enabled != 0,
         settings.fog_enabled != 0, lighting.values.fog_distance, settings.upscaler_mode,
         settings.fsr_sharpening != 0, settings.fsr_sharpness, settings.fsr_render_scale,
         settings.fsr_dynamic_resolution != 0, settings.fsr_min_scale, settings.fsr_max_scale,
         settings.fsr_target_fps, settings.ray_tracing_enabled != 0});
    auto avatar = player_presentation(player, controls, camera, double(now - start) / 1e9, 0.0, 0);
    avatar.visible = true;
    graphics::open_world_renderer_set_player(renderer, avatar);
    const auto render_start = SDL_GetTicksNS();
    // Qualification captures wait for map warmup so they show the settled view.
    graphics::open_world_renderer_set_capture_enabled(renderer, frames >= 180);
    const bool rendered = !(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
    if (rendered) {
      const bool map_ready = graphics::open_world_renderer_map_ready(renderer);
      const bool frame_ok = map_ready
          ? graphics::open_world_renderer_render(renderer, camera)
          : graphics::open_world_renderer_render_menu(renderer);
      if (!frame_ok) {
        std::fprintf(stderr, "World frame failed: %s\n", graphics::open_world_renderer_status(renderer));
        result = 1;
        break;
      }
    } else {
      SDL_Delay(10);
    }
    sample.render_ms = frame_profile_elapsed_ms_since(render_start);
    ++frames;
    const bool cli_capture = !options.capture_ui.empty() && frames == 5;
    if (cli_capture) {
      char* directory = SDL_GetPrefPath("ZSGStudios", "Octaryn");
      if (directory) {
        auto folder = std::filesystem::path(reinterpret_cast<const char8_t*>(directory)) / "ui-captures";
        SDL_free(directory);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const auto path = folder / (options.capture_ui + ".bmp");
        const auto utf8 = path.generic_u8string();
        if (!graphics::open_world_renderer_capture_ui(renderer, reinterpret_cast<const char*>(utf8.c_str())))
          std::fprintf(stderr, "UI capture failed\n");
      }
    }
    const auto stats = graphics::open_world_renderer_stats(renderer);
    if (map_loading && stats.map_ready) {
      map_loading = false;
      SDL_SetWindowTitle(window, "ZSG Engine");
      std::printf("map_loading complete\n");
      std::fflush(stdout);
    }
    const auto sleep_start = SDL_GetTicksNS();
    const auto delay = pacing.remaining_ns(now, sleep_start, options.benchmark_hidden && !benchmark_uncapped ? 30 : settings.frame_cap_fps,
        !options.benchmark_hidden && settings.present_mode_index == 1, uncapped);
    if (delay) {
      SDL_DelayNS(delay);
      sample.fps_cap_sleep_ms = frame_profile_elapsed_ms_since(sleep_start);
    }
    const auto completed = SDL_GetTicksNS();
    sample.total_ms = frame_profile_elapsed_ms(last_complete, completed);
    last_complete = completed;
    timing_log.frame(sample, stats, camera);
    if (stats.map_ready) camera_motion.record(stats.frames, frames, camera);
    if (options.validate_frame_pacing)
      pacing_report.frame(delay, sample.fps_cap_sleep_ms, sample.total_ms);
    profile.frame(window, sample, player, stats, "map");
    if (options.frame_limit > 0 && frames >= static_cast<unsigned>(options.frame_limit)) break;
    if (!stats.map_ready && now - start > 60000000000ull) {
      std::fprintf(stderr, "Map load timed out: %s\n", graphics::open_world_renderer_status(renderer));
      result = 1;
      break;
    }
  }
  if (options.validate_frame_pacing)
    pacing_report.report(pacing, options.benchmark_hidden ? 30 : controls.ui.frame_cap_fps,
        !options.benchmark_hidden && controls.ui.present_mode_index == 1);
  if (!graphics::open_world_renderer_flush(renderer)) {
    std::fprintf(stderr, "World graphics completion failed\n");
    result = 1;
  }
  const auto stats = graphics::open_world_renderer_stats(renderer);
  const auto metrics = profile.snapshot().metrics;
  profile.report_slow_frames();
  std::printf("world_profile average_ms=%.3f low_1pct_fps=%.2f worst_ms=%.3f samples=%llu\n",
              metrics.average.ms, metrics.low_1pct.fps, metrics.worst.ms,
              static_cast<unsigned long long>(metrics.sample_count));
  std::printf("open_world_exit mode=map code=%d frames=%u map_primitives=%u\n",
              result, frames, stats.map_primitives);
  std::fflush(stdout);
  return SessionOutcome{false, result};
}

}
