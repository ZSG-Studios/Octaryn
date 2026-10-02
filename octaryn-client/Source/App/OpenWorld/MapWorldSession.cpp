#include "WorldSession.h"
#include "../Validation/SceneTransitionRoute.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "Camera.h"
#include "LocalSession.h"
#include "ScenePhysicsPresentation.h"
#include "LoadingScreen.h"
#include "ModuleHost.h"
#include "../Startup/ModuleStartup.h"
#include "../Validation/ScenePhysicsRoute.h"
#include "GraphicsHost.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "../../Rendering/RenderBackend/WorldStartupReadiness.h"
#include "UiData.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "Menu.h"
#include "ModuleActionValidation.h"
#include "PlayerView.h"
#include "FramePacing.h"
#include "ResponsiveFrameWait.h"
#include "PostRenderTrace.h"
#include "FramePacingDisplay.h"
#include "FramePacingReport.h"
#include "FrameTimingLog.h"
#include "MapMotionValidation.h"
#include "ScenePreviewCamera.h"
#include "MapTileValidation.h"
#include "../Validation/GameplayRoute.h"
#include "../Validation/AudioWorkload.h"
#include "../Startup/InitialPlayable.h"
#include "TemporalValidation.h"
#include "../../Rendering/Performance/PerformanceProfile.h"
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
#include "HostExports.h"
#endif

#include <cmath>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>

namespace octaryn::client::app {
namespace graphics = octaryn::client::rendering;

// Mesh-map session with server-owned movement and game-module actions/events.
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
  bool menu_loading = true;
  if (!ctx.show_loading) game_ui->show_loading("Starting authoritative server...");
  const bool transitioning=host::scene_transition_mailbox.state==OCTARYN_TRANSITION_LOADING;
  game_ui->set_loading_cancelable(ctx.show_loading && !transitioning);
  ::camera camera_settings{};
  camera_init(&camera_settings, CAMERA_PROJECTION_PERSPECTIVE);
  LocalPlayerPose pose{};
  bool player_ready = false;
  unsigned frames = 0;
  bool ui_captured = false;
  ScenePhysicsPresentation scene_physics;ScenePhysicsRoute physics_route(SDL_getenv("OCTARYN_CLIENT_SCENE_PHYSICS_ROUTE"),options.benchmark_hidden,options.frame_limit>0 || options.benchmark_seconds>0);
  const char* capture_ready_env=SDL_getenv("OCTARYN_CLIENT_CAPTURE_READY_FRAME");
  const unsigned capture_ready_frame=capture_ready_env?unsigned(std::clamp(std::atoi(capture_ready_env),0,10000)):0;
  int result = 0;
  bool disconnect_requested = false;
  std::optional<host::SceneTransitionRequest> transition;
  bool ui_validation_done = false;
  std::fprintf(stderr,"map_session_setup stage=module_actions\n");game_ui->enable_module_actions();
  ModuleActionValidation module_validation(options.validate_module_actions);
  std::uint64_t benchmark_ready_ns = 0;
  FramePacing pacing;
  std::unique_ptr<FrameTimingLog> local_timing;
  if(!ctx.timing_log)local_timing=std::make_unique<FrameTimingLog>(SDL_getenv("OCTARYN_CLIENT_FRAME_TIMING_PATH"));
  auto& timing_log=ctx.timing_log?*ctx.timing_log:*local_timing;
  std::fprintf(stderr,"map_session_setup stage=audio\n");AudioWorkload audio_workload(options.benchmark_hidden,ctx.audio?ctx.audio->get():nullptr);
  InitialPlayable initial_playable;
  std::fprintf(stderr,"map_session_setup stage=map_motion\n");MapMotionValidation camera_motion(options.benchmark_hidden,options.benchmark_seconds,
      options.frame_limit,SDL_getenv("OCTARYN_CLIENT_MAP_CAMERA_MOTION"),
      SDL_getenv("OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH"));
  const char* sampling_env=SDL_getenv("OCTARYN_CLIENT_FIXED_SAMPLING");
  const bool fixed_sampling=options.benchmark_hidden &&
      (options.frame_limit>0 || options.benchmark_seconds>0) && camera_motion.locked_scene() &&
      sampling_env && *sampling_env=='1';
  if(fixed_sampling)std::printf("map_validation_sampling fixed=1 index=ready_frame presentation_delta_ms=16.666667 timing_qualification=0\n");
  TemporalValidation temporal;
  std::fprintf(stderr,"map_session_setup stage=tile_motion\n");MapTileValidation tile_motion(options.benchmark_hidden,options.benchmark_seconds,
      options.frame_limit,SDL_getenv("OCTARYN_CLIENT_TILE_CAMERA_ROUTE"),
      SDL_getenv("OCTARYN_CLIENT_TILE_CAMERA_ROUTE_PATH"));
  std::fprintf(stderr,"map_session_setup stage=gameplay_route\n");GameplayRoute gameplay_route(options.benchmark_hidden,options.benchmark_seconds,options.frame_limit);
  ScenePreviewCamera preview_camera(SDL_getenv("OCTARYN_CLIENT_SCENE_PREVIEW_CAMERA"));
  if(ctx.view_origin)preview_camera.set_origin(*ctx.view_origin);
  const char* camera_route=SDL_getenv("OCTARYN_CLIENT_MAP_CAMERA_MOTION");
  if(gameplay_route.authored() && (camera_motion.locked_scene() || tile_motion.enabled() ||
      (camera_route && (std::strcmp(camera_route,"1")==0 || std::strcmp(camera_route,"loop")==0))))
    throw std::runtime_error("Gameplay qualification cannot combine with camera-only routes");
  if(preview_camera.enabled() && (gameplay_route.authored() || options.validate_module_actions ||
      options.validate_temporal || options.validate_world_items || options.validate_block_actions))
    throw std::runtime_error("Scene preview camera cannot qualify gameplay or temporal behavior");
  const char* uncapped_env=SDL_getenv("OCTARYN_CLIENT_BENCHMARK_UNCAPPED");
  const bool benchmark_uncapped=options.benchmark_hidden && uncapped_env && *uncapped_env=='1';
  unsigned benchmark_frame_cap=30;
  if(options.benchmark_hidden) {
    if(const char* cap=SDL_getenv("OCTARYN_CLIENT_BENCHMARK_FRAME_CAP");cap && *cap) {
      const auto* end=cap+std::char_traits<char>::length(cap);
      const auto parsed=std::from_chars(cap,end,benchmark_frame_cap);
      if(parsed.ec!=std::errc{} || parsed.ptr!=end || benchmark_frame_cap<30 || benchmark_frame_cap>240)
        throw std::runtime_error("OCTARYN_CLIENT_BENCHMARK_FRAME_CAP requires an integer from 30 through 240");
    }
    std::printf("map_benchmark_pacing frame_cap=%u uncapped=%u\n",benchmark_frame_cap,unsigned(benchmark_uncapped));
  }
  const bool performance_uncapped=graphics::requested_performance_profile()==graphics::PerformanceProfile::HQ200;
  std::fprintf(stderr,"map_session_setup stage=pacing_report\n");FramePacingReport pacing_report(options.validate_frame_pacing ? SDL_getenv("OCTARYN_CLIENT_PACING_PROFILE_PATH") : nullptr);
  ResponsiveFrameWait frame_wait;PostRenderTrace post_render;
  const bool uncapped = performance_uncapped || benchmark_uncapped || (!options.benchmark_hidden && !options.validate_frame_pacing &&
      (options.frame_limit > 0 || options.benchmark_seconds > 0));
  std::fprintf(stderr,"map_session_setup stage=display_refresh\n");if (!uncapped) pacing.update_display([&] { return frame_pacing_refresh_rate(window); });
  auto last = SDL_GetTicksNS();
  auto last_complete = last;
  auto start = last;
  std::printf("open_world_start mode=map shader=slang backend=slang_rhi authority=%s\n",
      ctx.remote_authority ? "remote_server" : "local_server");
  std::fflush(stdout);
  static host::GraphicsHost graphics_host;
  graphics_host={window,&controls.ui,renderer};
  const host::ModuleHostHooks module_hooks{
      ctx.audio != nullptr ? ctx.audio->get() : nullptr, game_ui,&graphics_host,&controls.running,&session};
  const int module_host_result = start_world_module(ctx,module_hooks);last=last_complete=start=SDL_GetTicksNS();
  if (module_host_result < 0)
    std::fprintf(stderr, "Module host startup failed: %d\n", module_host_result);
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
    if(menu_loading && game_ui->take_loading_cancel()) {
      disconnect_requested=true;std::puts("world_loading_cancel phase=player_wait");break;
    }
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
    if(gameplay_route.apply(input,elapsed,player_ready,session)) {
      controls.yaw=input.yaw;controls.pitch=input.pitch;controls.flying=input.flying;
    }
    // Prediction ignores nonfinite local look until the first server pose arrives.
    // Commands serialize its finite body angles, never these input sentinels.
    if(!player_ready)input.yaw=input.pitch=std::numeric_limits<float>::quiet_NaN();
    take_jump_input(controls, input);
    preview_camera.suppress(input);
    if(preview_camera.enabled())controls.actions.clear();
    const auto sim_start = SDL_GetTicksNS();
    session.update(input, elapsed);
    if(!graphics::open_world_renderer_set_items(renderer,session.world_items(),session.world_items_revision())) {
      std::fprintf(stderr,"item_render_snapshot_failed\n");result=1;break;
    }
    if(!audio_workload.step(graphics::open_world_renderer_stats(renderer).frames,elapsed,player_ready)) {
      std::fprintf(stderr,"audio_workload_failed\n");result=1;break;
    }
    // The module owns inventory; the map UI submits intent without reserving
    // items in the old client block palette.
    {
      if (player_ready && !menu_loading) module_validation.start(*game_ui);
      std::string action;
      while (game_ui->take_module_action(action)) {
        if (!session.publish_ui_action(action)) {
          game_ui->show_notification("Action queue full; try again.");
          std::fprintf(stderr,"module_action_rejected action=%s\n",action.c_str());
          if (options.validate_module_actions) result=1;
        }
      }
    }
    {
      std::uint64_t event_id{}, event_kind{}, event_p1{}, event_p2{};
      while (session.poll_module_event(event_id, event_kind, event_p1, event_p2)) {
        module_validation.event(event_id,event_kind,event_p1,event_p2);
        if (options.validate_module_actions) std::printf("module_event id=%llu kind=%llu item=%llu count=%llu\n",
            static_cast<unsigned long long>(event_id),static_cast<unsigned long long>(event_kind),
            static_cast<unsigned long long>(event_p1),static_cast<unsigned long long>(event_p2));
        switch (event_kind) {
          case 1u:
            game_ui->show_notification("Picked up "+std::to_string(event_p2)+" item(s)");
            break;
          case 2u:
            game_ui->show_notification("Dropped "+std::to_string(event_p2)+" item(s)");
            break;
          case 3u: // ItemTargeted: crosshair highlight; id 0 clears.
            game_ui->show_item_target(static_cast<std::uint32_t>(event_p1),
                                      static_cast<std::uint32_t>(event_p2));
            break;
          default: break;
        }
      }
    }
    {
      const bool route_use=ctx.transition_route && ctx.transition_route->input(ctx.scene_asset,player_ready && !menu_loading);
      const auto physics_edges=physics_route.input(session,ctx.scene_asset,player_ready && !menu_loading);if(physics_route.done() || (ctx.transition_route && ctx.transition_route->done()))break;
      octaryn_host_input_snapshot module_input{};
      module_input.version = 1u;
      module_input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
      module_input.flags = (input.jump_events.count > 0 ? 1u : 0u) |
          (input.sprint ? 2u : 0u) | (input.flying ? 4u : 0u) | ((controls.menu_pressed || (ctx.transition_route && ctx.transition_route->menu_edge())) ? 32u : 0u) | ((controls.use_pressed || route_use) ? 64u : 0u) | (controls.grab_pressed ? 128u : 0u);
      module_input.flags|=physics_edges|(physics_route.menu_edge()?32u:0u);module_input.controller = 1u;
      module_input.move_x = static_cast<float>(input.right) - static_cast<float>(input.left);
      module_input.move_y = static_cast<float>(input.up) - static_cast<float>(input.down);
      module_input.move_z = static_cast<float>(input.forward) - static_cast<float>(input.backward);
      module_input.camera_pitch = input.pitch;
      module_input.camera_yaw = input.yaw;
      module_input.relative_mouse = 1;
      const auto module_frame=ctx.module_frame?(*ctx.module_frame)++:frames;
      LocalPlayerPose actor_pose{};
      if(session.player_pose(actor_pose)) {
        graphics::WorldCamera actor;actor.x=actor_pose.x;actor.y=actor_pose.y;actor.z=actor_pose.z;
        graphics::open_world_renderer_set_tile_anchor(renderer,actor,true);
      }
      host::module_host_tick(module_frame, elapsed, module_input);
      if(!scene_physics.update(renderer,session)) {result=1;break;}
      host::SceneTransitionRequest requested;
      if(host::scene_transition_take(requested)) {transition=std::move(requested);break;}
    }
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
        const auto ready_time=SDL_GetTicksNS();
        std::printf("authoritative_player_ready eye=%.3f,%.3f,%.3f yaw=%.6f pitch=%.6f sdl_uptime_ms=%.3f session_elapsed_ms=%.3f\n",
            pose.x,pose.y,pose.z,pose.yaw,pose.pitch,double(ready_time)/1e6,double(ready_time-start)/1e6);
        std::fflush(stdout);
      }
    }
    const float zoom = static_cast<float>(1u << controls.zoom);
    const float fov = 2 * std::atan(std::tan(camera_settings.vertical_field_of_view_radians / 2) / zoom);
    auto camera = player_camera_map(pose, controls, fov);
    if(player_ready)preview_camera.apply(camera,controls,elapsed);
    if(player_ready)camera_motion.apply(camera,frames);
    if(player_ready)tile_motion.apply(camera,frames);
    if(ctx.transition_route)ctx.transition_route->camera(camera,ctx.scene_asset);physics_route.camera(camera,ctx.scene_asset);
    host::scene_transition_publish_view(ctx.scene_asset,{camera.x,camera.y,camera.z,camera.yaw,camera.pitch},
        player_ready && !menu_loading && !ctx.remote_authority && (!options.benchmark_hidden || physics_route.active() || (ctx.transition_route && ctx.transition_route->active())));
    if (options.validate_temporal) temporal.camera(camera);
    auto ui = graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(ui, profile.snapshot());
    SDL_GetWindowSizeInPixels(window, &width, &height);
    const auto ui_start = SDL_GetTicksNS();
    const auto resolution_stats = graphics::open_world_renderer_stats(renderer);
    if (options.validate_temporal)
      temporal.begin_frame(window, controls.ui, resolution_stats, double(now - start) / 1e9);
    game_ui->set_render_resolution(resolution_stats.render_width, resolution_stats.render_height,
        resolution_stats.display_width, resolution_stats.display_height);
    game_ui->update(ui, 0, width, height);
    sample.ui_ms = frame_profile_elapsed_ms_since(ui_start);
    // Explicit UI qualification: run the document contract once the world
    // session has produced a few live frames. The inventory contract needs a
    // content catalog, which the map platform does not ship.
    if (options.validate_ui && player_ready && frames == 5 && !ui_validation_done) {
      ui_validation_done = true;
      if (!game_ui->validate_contract() || !game_ui->validate_item_target_contract()) {
        result = 1;
        break;
      }
    }
    graphics::open_world_renderer_set_lighting(renderer, lighting.values);
    graphics::open_world_renderer_set_lighting_debug(renderer, sanitize_lighting_debug(lighting.debug_view));
    graphics::open_world_renderer_set_reflection_quality(renderer, controls.ui.reflection_quality);
    graphics::open_world_renderer_set_shadow_quality(renderer, controls.ui.shadow_quality);
    graphics::open_world_renderer_set_trace_ranges(renderer, float(controls.ui.shadow_distance),
        float(controls.ui.reflection_distance));
    graphics::open_world_renderer_set_present(renderer, options.benchmark_hidden ? 0 : controls.ui.present_mode_index);
    const auto& settings = controls.ui;
    graphics::open_world_renderer_set_scene(renderer,
        {camera_motion.locked_scene()?.5:pose.world_day_fraction, camera_motion.locked_scene()?0:pose.source_seconds, settings.sky_gradient_enabled != 0,
         settings.stars_enabled != 0, settings.sun_enabled != 0, settings.moon_enabled != 0,
         settings.pbr_enabled != 0, settings.pom_enabled != 0, settings.clouds_enabled != 0,
         settings.fog_enabled != 0, lighting.values.fog_distance, settings.upscaler_mode,
         settings.fsr_sharpening != 0, settings.fsr_sharpness, settings.fsr_render_scale,
         settings.fsr_dynamic_resolution != 0, settings.fsr_min_scale, settings.fsr_max_scale,
         settings.fsr_target_fps, settings.ray_tracing_enabled != 0 && player_ready});
    const auto render_start = SDL_GetTicksNS();
    // Captures arm once authority drives the view; the renderer-side minimum
    // frame (OCTARYN_CLIENT_CAPTURE_MIN_FRAME) owns warmup gating. Temporal
    // qualification additionally gates on its final-phase capture request.
    graphics::open_world_renderer_set_capture_enabled(renderer,
        player_ready && frames>=capture_ready_frame && tile_motion.capture_ready(frames) &&
        (!options.validate_temporal || temporal.capture_ready()));
    const bool rendered = !(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
    graphics::open_world_renderer_set_validation_sampling(renderer,fixed_sampling && player_ready,frames);
    if (rendered) {
      // Until authority supplies the camera, draw only the loading UI. Rendering
      // the origin wastes GPU work and presents a misleading below-map view.
      const bool frame_ok = player_ready
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
    const auto& pending_transition=host::scene_transition_mailbox;
    rendering::WorldStartupReadiness destination_ready;
    if(menu_loading || pending_transition.state==OCTARYN_TRANSITION_LOADING)
      destination_ready=rendering::open_world_renderer_startup_readiness(renderer,camera);
    if(rendered && player_ready && destination_ready.tiles.requested>0 &&
        destination_ready.tiles.requested_ready && destination_ready.ray_ready &&
        pending_transition.state==OCTARYN_TRANSITION_LOADING &&
        (pending_transition.request.asset_id==ctx.scene_asset || !ctx.transition_error.empty()))
    {
      const auto revision=pending_transition.revision;
      const auto queued=pending_transition.request.queued_at_ns;
      const bool restored=!ctx.transition_error.empty();
      host::scene_transition_complete(revision,!restored,ctx.transition_error);
      ctx.transition_error.clear();
      std::printf("scene_transition_ready revision=%llu asset=%s authority=1 renderer=1 restored=%u elapsed_ms=%.3f requested=%u resident=%u requested_ready=1\n",
          static_cast<unsigned long long>(revision),ctx.scene_asset.c_str(),unsigned(restored),queued?double(SDL_GetTicksNS()-queued)/1e6:0,
          destination_ready.tiles.requested,destination_ready.tiles.resident);
    }
    sample.render_ms = frame_profile_elapsed_ms_since(render_start);
    post_render.begin(resolution_stats.frames,frames);
    post_render.stage("post_render_validation");
    if(menu_loading && validate_loading_cancel(window,*game_ui,"Waiting for player")) {
      game_ui->take_loading_cancel();disconnect_requested=true;
      std::puts("world_loading_cancel phase=player_wait");break;
    }
    if(rendered && player_ready)initial_playable.presented(renderer,camera,resolution_stats.frames,session);
    if (options.validate_temporal) {
      temporal.frame_rendered(resolution_stats, game_ui->context(), window,
          player_ready && resolution_stats.map_ready, rendered,
          graphics::open_world_renderer_captured(renderer), double(now - start) / 1e9);
      if (temporal.complete()) break;
    }
    const bool cli_capture = !options.capture_ui.empty() && !ui_captured && !menu_loading && frames>=6;
    if (cli_capture || game_ui->consume_ui_capture_request()) {
      if (cli_capture) ui_captured=true;
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
    post_render.stage("stats_begin");
    const auto stats = graphics::open_world_renderer_stats(renderer);
    post_render.stage("stats_end");
    if(rendered && player_ready && stats.map_ready)gameplay_route.record(stats.frames?stats.frames-1:0,session);
    if (menu_loading && SDL_getenv("OCTARYN_CLIENT_INPUT_GATE_TRACE") != nullptr) {
      static unsigned loading_trace = 0;
      if (loading_trace++ % 120 == 0)
        std::fprintf(stderr, "loading_gate player_ready=%d map_ready=%d menu_active=%u\n",
            player_ready ? 1 : 0, stats.map_ready ? 1 : 0, controls.ui.display_menu.active);
    }
    if (menu_loading && update_map_loading(*game_ui, controls.ui, window,
        player_ready, destination_ready.tiles.requested>0 && destination_ready.tiles.requested_ready &&
          destination_ready.ray_ready, session.status())) {
      menu_loading = false;
      if (options.show_menu) game_ui->show_pause_menu();
      else if (options.show_settings) display_menu_open(&controls.ui.display_menu);
      else if (options.show_fsr_settings) game_ui->show_fsr_settings();
      if (options.show_lighting) lighting.visible=true;
      if (options.show_inventory || options.show_creative) game_ui->show_inventory(options.show_creative);
    }
    if (!menu_loading && options.show_item_target) game_ui->show_item_target(1,3);
    const auto sleep_start = SDL_GetTicksNS();
    post_render.stage("pacing_begin");
    const auto frame_cap=!player_ready?60u:options.benchmark_hidden && !benchmark_uncapped?benchmark_frame_cap:settings.frame_cap_fps;
    const auto delay = pacing.remaining_ns(now, sleep_start, frame_cap,
        !options.benchmark_hidden && settings.present_mode_index == 1, uncapped && player_ready);
    if (delay) {
      post_render.stage("cap_sleep_begin",delay);
      FrameWaitReport wait_report;
      if(!frame_wait.sleep(delay,[]{return SDL_GetTicksNS();},wait_report)) {
        std::fprintf(stderr,"frame_pacing_wait_failed calls=%u result=%u\n",wait_report.waits,unsigned(wait_report.result));
        result=1;break;
      }
      post_render.stage("cap_sleep_end",delay,wait_report.actual_ns,wait_report.waits,
          wait_report.last_timeout_ms,unsigned(wait_report.result));
      sample.fps_cap_sleep_ms = frame_profile_elapsed_ms_since(sleep_start);
    }
    const auto completed = SDL_GetTicksNS();
    sample.total_ms = frame_profile_elapsed_ms(last_complete, completed);
    last_complete = completed;
    pacing_report.frame(delay, sample.fps_cap_sleep_ms, sample.total_ms);
    post_render.stage("timing_begin");
    timing_log.frame(sample, stats, camera);
    post_render.stage("timing_end");
    if(player_ready && stats.map_ready)camera_motion.record(stats.frames?stats.frames-1:0,frames,camera);
    if(player_ready && stats.map_ready)tile_motion.record(stats.frames?stats.frames-1:0,frames,camera,pose);
    profile.frame(window, sample, pose, stats, "map", preview_camera.enabled());
    post_render.stage("profile_end");
    if (player_ready && stats.map_ready) ++frames;
    if (!options.map_switch_worlds[0].empty() && frames >= 60) {
      disconnect_requested = true;
      break;
    }
    // Timed benchmarks measure from the first authoritative frame, not startup.
    if (options.benchmark_seconds > 0) {
      if (player_ready && !benchmark_ready_ns) benchmark_ready_ns = now;
      if (tile_motion.enabled()?tile_motion.duration_complete(options.benchmark_seconds):
          benchmark_ready_ns && double(now - benchmark_ready_ns) / 1e9 >= options.benchmark_seconds) break;
    }
    if (options.frame_limit > 0 && frames >= static_cast<unsigned>(options.frame_limit)) break;
    // Rejoin qualification drives the menu disconnect action itself: end each
    // session deterministically once the authoritative pose has been rendered.
    if (options.validate_session_rejoin && player_ready && frames >= 240) {
      disconnect_requested = true;
      break;
    }
    if (!player_ready && now - start > 60000000000ull) {
      std::fprintf(stderr, "Map startup timed out: %s\n", session.status().c_str());
      result = 1;
      break;
    }
  }
  if (options.validate_frame_pacing)
    pacing_report.report(pacing, options.benchmark_hidden ? benchmark_frame_cap : controls.ui.frame_cap_fps,
        !options.benchmark_hidden && controls.ui.present_mode_index == 1);
  if (!graphics::open_world_renderer_flush(renderer)) {
    std::fprintf(stderr, "World graphics completion failed\n");
    result = 1;
  }
  const auto& pending_scene=host::scene_transition_mailbox;
  const bool startup_pending=ctx.show_loading && pending_scene.state==OCTARYN_TRANSITION_LOADING &&
      pending_scene.request.asset_id==ctx.scene_asset;
  const bool loading_failed=ctx.show_loading && result!=0 && (menu_loading || startup_pending);
  if(!transition && startup_pending && (result!=0 || disconnect_requested)) {
    const auto error=result!=0?"Initial game presentation failed: "+session.status():"Game loading canceled.";
    host::scene_transition_complete(pending_scene.revision,false,error);
    std::fprintf(stderr,"module_startup_failed error=%s\n",error.c_str());
  }
  if(!transition)host::module_host_stop();
  if (!module_validation.finish()) result=1;
  if (!gameplay_route.finish()) result=1;
  game_ui->clear_item_target();
  const auto stats = graphics::open_world_renderer_stats(renderer);
  const auto metrics = profile.snapshot().metrics;
  profile.report_slow_frames();
  std::printf("world_profile average_ms=%.3f low_1pct_fps=%.2f worst_ms=%.3f samples=%llu\n",
              metrics.average.ms, metrics.low_1pct.fps, metrics.worst.ms,
              static_cast<unsigned long long>(metrics.sample_count));
  std::printf("open_world_exit mode=map code=%d frames=%u map_primitives=%u\n",
              result, frames, stats.map_primitives);
  std::fflush(stdout);
  return SessionOutcome{disconnect_requested,result,loading_failed?
      "World loading failed: "+session.status():std::string{},std::move(transition)};
}

} // namespace octaryn::client::app
