#include "WorldSession.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "Camera.h"
#include "LocalSession.h"
#include "LoadingScreen.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "UiData.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "PlayerView.h"
#include "FramePacing.h"
#include "FramePacingDisplay.h"
#include "FramePacingReport.h"
#include "FrameTimingLog.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace octaryn::client::app {
namespace graphics = octaryn::client::rendering;

// Mesh-map session: walks a Blender-exported GLB world with the authoritative
// server publishing the pose. No voxel streaming, block edits, selection,
// inventory, or streamed world items exist in this mode.
SessionOutcome run_map_world_session(WorldSession& ctx, LocalSession& session) {
  SDL_Window* window = ctx.window;
  const WorldRunOptions& options = *ctx.options;
  WorldProfile& profile = *ctx.profile;
  LightingPanel& lighting = *ctx.lighting;
  WorldControls& controls = *ctx.controls;
  unsigned& radius = *ctx.radius;
  int& width = *ctx.width;
  int& height = *ctx.height;
  auto* renderer = ctx.renderer;
  GameUi* game_ui = ctx.ui;
  bool menu_loading = ctx.show_loading;
  ::camera camera_settings{};
  camera_init(&camera_settings, CAMERA_PROJECTION_PERSPECTIVE);
  LocalPlayerPose pose{};
  bool player_ready = false;
  unsigned frames = 0;
  unsigned ui_frames = 0;
  int result = 0;
  bool disconnect_requested = false;
  FramePacing pacing;
  FrameTimingLog timing_log(SDL_getenv("OCTARYN_CLIENT_FRAME_TIMING_PATH"));
  FramePacingReport pacing_report(options.validate_frame_pacing ? SDL_getenv("OCTARYN_CLIENT_PACING_PROFILE_PATH") : nullptr);
  const bool uncapped = !options.validate_frame_pacing &&
      (options.frame_limit > 0 || options.benchmark_seconds > 0);
  if (!uncapped) pacing.update_display([&] { return frame_pacing_refresh_rate(window); });
  auto last = SDL_GetTicksNS();
  auto last_complete = last;
  const auto start = last;
  std::printf("open_world_start mode=map shader=slang backend=slang_rhi authority=%s\n",
      ctx.remote_authority ? "remote_server" : "local_server");
  std::fflush(stdout);
  while (controls.running) {
    const auto now = SDL_GetTicksNS();
    const double elapsed = static_cast<double>(now - last) / 1e9;
    last = now;
    frame_profile_sample sample{};
    sample.total_ms = static_cast<float>(elapsed * 1000.0);
    const auto event_start = SDL_GetTicksNS();
    read_world_controls(window, controls, options.benchmark_seconds <= 0);
    if (controls.display_changed) pacing.invalidate_display();
    if (!uncapped) pacing.update_display([&] { return frame_pacing_refresh_rate(window); });
    sample.misc_ms = frame_profile_elapsed_ms_since(event_start);
    if (!controls.running) break;
    if (controls.ui.display_menu.action_requested == DISPLAY_MENU_ACTION_DISCONNECT_SESSION) {
      controls.ui.display_menu.action_requested = DISPLAY_MENU_ACTION_NONE;
      disconnect_requested = true;
      break;
    }
    const auto requested_radius = static_cast<unsigned>(controls.ui.render_distance);
    if (requested_radius != radius) {
      radius = requested_radius;
      session.set_radius(radius);
      std::printf("render_distance_changed radius=%u\n", radius);
      std::fflush(stdout);
    }
    const auto& move = controls.movement;
    LocalPlayerInput input{move.move_forward != 0, move.move_backward != 0,
                           move.move_left != 0, move.move_right != 0,
                           move.move_up != 0, move.move_down != 0,
                           move.sprint != 0, controls.flying, controls.yaw, controls.pitch};
    take_jump_input(controls, input);
    const auto sim_start = SDL_GetTicksNS();
    session.update(input, elapsed);
    if (controls.time_hour_steps) session.step_world_hours(controls.time_hour_steps);
    sample.sim_ms = frame_profile_elapsed_ms_since(sim_start);
    if (!session.running()) {
      std::fprintf(stderr, "Local server stopped: %s\n", session.status().c_str());
      result = 1;
      break;
    }
    if (session.player_pose(pose)) {
      if (!player_ready) {
        controls.yaw = pose.yaw;
        controls.pitch = pose.pitch;
        controls.flying = pose.flying;
        player_ready = true;
        game_ui->hide_voxel_hud();
        std::printf("authoritative_player_ready eye=%.3f,%.3f,%.3f\n", pose.x, pose.y, pose.z);
        std::fflush(stdout);
      }
    }
    const float zoom = static_cast<float>(1u << controls.zoom);
    const float fov = 2 * std::atan(std::tan(camera_settings.vertical_field_of_view_radians / 2) / zoom);
    const auto camera = player_camera_map(pose, controls, fov);
    auto ui = graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(ui, profile.snapshot());
    SDL_GetWindowSizeInPixels(window, &width, &height);
    const auto ui_start = SDL_GetTicksNS();
    const auto resolution_stats = graphics::open_world_renderer_stats(renderer);
    game_ui->set_render_resolution(resolution_stats.render_width, resolution_stats.render_height,
        resolution_stats.display_width, resolution_stats.display_height);
    game_ui->update(ui, 0, width, height);
    sample.ui_ms = frame_profile_elapsed_ms_since(ui_start);
    graphics::open_world_renderer_set_lighting(renderer, lighting.values);
    graphics::open_world_renderer_set_lighting_debug(renderer, sanitize_lighting_debug(lighting.debug_view));
    graphics::open_world_renderer_set_lighting_quality(renderer, controls.ui.lighting_quality);
    graphics::open_world_renderer_set_raster_shadows(renderer, controls.ui.raster_sun_shadows);
    graphics::open_world_renderer_set_trace_ranges(renderer, float(controls.ui.shadow_distance),
        float(controls.ui.reflection_distance));
    graphics::open_world_renderer_set_present(renderer, controls.ui.present_mode_index);
    const auto& settings = controls.ui;
    graphics::open_world_renderer_set_scene(renderer,
        {pose.world_day_fraction, pose.source_seconds, settings.sky_gradient_enabled != 0,
         settings.stars_enabled != 0, settings.sun_enabled != 0, settings.moon_enabled != 0,
         settings.pbr_enabled != 0, settings.pom_enabled != 0, settings.clouds_enabled != 0,
         settings.fog_enabled != 0, lighting.values.fog_distance, settings.upscaler_mode,
         settings.fsr_sharpening != 0, settings.fsr_sharpness, settings.fsr_render_scale,
         settings.fsr_dynamic_resolution != 0, settings.fsr_min_scale, settings.fsr_max_scale,
         settings.fsr_target_fps, settings.ray_tracing_enabled != 0});
    if (!graphics::open_world_renderer_set_ddgi_range(renderer, controls.ui.gi_voxel_radius,
        controls.ui.gi_coarse_radius)) {
      std::fprintf(stderr, "GI range apply failed: %s\n", graphics::open_world_renderer_status(renderer));
      result = 1;
      break;
    }
    auto avatar = player_presentation(pose, controls, camera, double(now - start) / 1e9, 0.0, 0);
    avatar.visible = player_ready;
    graphics::open_world_renderer_set_player(renderer, avatar);
    const auto render_start = SDL_GetTicksNS();
    const bool rendered = !(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
    if (rendered) {
      if (!graphics::open_world_renderer_render(renderer, camera)) {
        std::fprintf(stderr, "World frame failed: %s\n", graphics::open_world_renderer_status(renderer));
        result = 1;
        break;
      }
    } else {
      SDL_Delay(10);
    }
    sample.render_ms = frame_profile_elapsed_ms_since(render_start);
    ++ui_frames;
    const bool cli_capture = !options.capture_ui.empty() && ui_frames == 5;
    if (cli_capture || game_ui->consume_ui_capture_request()) {
      char* directory = SDL_GetPrefPath("ZSGStudios", "Octaryn");
      if (directory) {
        auto folder = std::filesystem::path(reinterpret_cast<const char8_t*>(directory)) / "ui-captures";
        SDL_free(directory);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const std::string name = cli_capture ? options.capture_ui : "ui-" + std::to_string(SDL_GetTicksNS() / 1000000ull);
        const auto path = folder / (name + ".bmp");
        const auto utf8 = path.generic_u8string();
        if (!graphics::open_world_renderer_capture_ui(renderer, reinterpret_cast<const char*>(utf8.c_str())))
          std::fprintf(stderr, "UI capture failed\n");
      }
    }
    const auto stats = graphics::open_world_renderer_stats(renderer);
    if (menu_loading && update_map_loading(*game_ui, controls.ui, window,
        player_ready, stats.map_ready, session.status()))
      menu_loading = false;
    const auto sleep_start = SDL_GetTicksNS();
    const auto delay = pacing.remaining_ns(now, sleep_start, settings.frame_cap_fps,
        settings.present_mode_index == 1, uncapped);
    if (delay) {
      SDL_DelayNS(delay);
      sample.fps_cap_sleep_ms = frame_profile_elapsed_ms_since(sleep_start);
    }
    const auto completed = SDL_GetTicksNS();
    sample.total_ms = frame_profile_elapsed_ms(last_complete, completed);
    last_complete = completed;
    timing_log.frame(sample, stats, camera);
    if (options.validate_frame_pacing)
      pacing_report.frame(delay, sample.fps_cap_sleep_ms, sample.total_ms, stats.columns, stats.pending_meshes);
    profile.frame(window, sample, pose, stats, "map", session.movement_stats());
    if (player_ready && stats.map_ready) ++frames;
    if (options.frame_limit > 0 && frames >= static_cast<unsigned>(options.frame_limit)) break;
    if (!player_ready && now - start > 60000000000ull) {
      std::fprintf(stderr, "Map startup timed out: %s\n", session.status().c_str());
      result = 1;
      break;
    }
  }
  if (options.validate_frame_pacing)
    pacing_report.report(pacing, controls.ui.frame_cap_fps, controls.ui.present_mode_index == 1);
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
  return SessionOutcome{disconnect_requested, result};
}

} // namespace octaryn::client::app
