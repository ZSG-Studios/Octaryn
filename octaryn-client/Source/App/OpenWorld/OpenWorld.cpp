#include "OpenWorld.h"
#include "Controls.h"
#include "Camera.h"
#include "LocalSession.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "WorldStream.h"
#include "BlockInteraction.h"
#include "RuntimeSettings.h"
#include "UiData.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "PlayerView.h"
#include "SelectionTarget.h"
#include "ActionAudio.h"
#include "ActionSounds.h"
#include "ActionFeedback.h"
#include "InventoryActions.h"
#include "DistanceValidation.h"
#include "WorldItemActions.h"
#include "WorldItemsValidation.h"
#include "TemporalValidation.h"
#include "StreamingBenchmark.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
namespace fs = std::filesystem;
namespace graphics = octaryn::client::rendering;
namespace presentation = octaryn::client::world_presentation;

fs::path utf8_path(const char* text) {
  if (!text) throw std::runtime_error("Missing application path");
  return fs::path(reinterpret_cast<const char8_t*>(text));
}

fs::path data_root(const fs::path& bundle) {
  const auto root = bundle.parent_path().parent_path().parent_path().parent_path();
  if (fs::exists(root / "CMakeLists.txt")) return root;
  char* pref = SDL_GetPrefPath("ZSGStudios", "Octaryn");
  if (!pref) throw std::runtime_error(SDL_GetError());
  fs::path path = utf8_path(pref);
  SDL_free(pref);
  return path;
}

int run_window(SDL_Window* window, const WorldRunOptions& options) {
  auto bundle = utf8_path(SDL_GetBasePath());
  if (bundle.filename().empty()) bundle = bundle.parent_path();
  const auto root = data_root(bundle);
  const char* override_path = SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  const auto world = override_path && *override_path ? utf8_path(override_path)
                                                    : root / "saves" / "open-world-v2";
  fs::create_directories(root / "logs" / "client");
  WorldProfile profile(root / "logs" / "client" / "open-world.csv");
  StreamingBenchmark streaming_benchmark(options.benchmark_streaming_speed);
  LightingPanel lighting(window);
  lighting.visible=options.show_lighting;
  WorldControls controls;
  controls.lighting=&lighting;
  controls.third_person=options.third_person;
  controls.shoulder=options.shoulder;
  runtime_controls_init(&controls.ui);
  controls.ui.render_distance=4;
  controls.ui.debug_overlay_enabled=options.show_diagnostics?1:0;
  if (options.benchmark_seconds <= 0 || options.benchmark_settings) runtime_settings_load(window, &controls.ui);
  if (options.render_distance>0) controls.ui.render_distance=options.render_distance;
  runtime_controls_set_max_render_distance(&controls.ui, 32);
  controls.ui.session_active = 1;
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  runtime_controls_refresh_menu(&controls.ui, window, width, height);
  if (options.show_settings) display_menu_open(&controls.ui.display_menu);
  unsigned radius = static_cast<unsigned>(controls.ui.render_distance);
  LocalSession session;
  const bool qualification=options.validate_world_items || options.validate_temporal;
  if (!session.start(bundle, world, radius, qualification?world/"logs"/"server":root/"logs"/"server")) {
    std::fprintf(stderr, "Local server startup failed: %s\n", session.status().c_str());
    return 1;
  }
  presentation::WorldStream stream(session.chunk_stream_path());
  presentation::WorldItemsClient world_items(world);
  WorldItemActions item_actions;
  WorldItemsValidation item_validation;
  TemporalValidation temporal_validation;
  presentation::BlockInteraction interaction;
  if (!interaction.load_catalog(bundle / "Data" / "Blocks" / "octaryn.basegame.blocks.json"))
    throw std::runtime_error("Cannot load the basegame block interaction catalog");
  audio::ActionAudioOwner audio_owner(audio::create_action_audio(
      load_action_sounds(bundle / "Assets" / "Audio" / "action-sounds.json")));
  const auto audio_status=audio::action_audio_status(audio_owner.get());
  std::printf("action_audio available=%u status=%s\n",audio_status.available?1u:0u,
      audio_status.message?audio_status.message:"unknown");
  std::fflush(stdout);
  std::unique_ptr<graphics::WorldRenderer, decltype(&graphics::open_world_renderer_destroy)> renderer_owner(
      graphics::open_world_renderer_create(window), graphics::open_world_renderer_destroy);
  auto* renderer = renderer_owner.get();
  if (!renderer) {
    std::fprintf(stderr, "Slang RHI world renderer initialization failed\n");
    return 1;
  }
  const char* palette_override=SDL_getenv("OCTARYN_CLIENT_INVENTORY_PATH");
  const auto palette=palette_override && *palette_override?utf8_path(palette_override):world/"client"/"inventory.json";
  const auto prior_palette=root/"settings"/"build-palette.json";
  if(!palette_override && !fs::exists(palette) && fs::exists(prior_palette))
    fs::copy_file(prior_palette,palette);
  auto game_ui=std::make_unique<GameUi>(window,graphics::open_world_renderer_ui_interface(renderer),
      bundle / "Assets" / "Ui",controls.ui,lighting,palette);
  controls.game_ui=game_ui.get();
  graphics::open_world_renderer_set_ui_context(renderer,game_ui->context());
  graphics::open_world_renderer_set_capture_enabled(renderer,!qualification);
  if (options.validate_ui) {
    game_ui->update(graphics::make_ui_draw_data(controls.ui),0,width,height);
    if (!game_ui->validate_contract()) throw std::runtime_error("RmlUi document contract validation failed");
    if (!game_ui->validate_fsr_contract()) throw std::runtime_error("FSR UI contract validation failed");
    if (!game_ui->validate_inventory_contract()) throw std::runtime_error("Inventory UI contract validation failed");
    std::puts("rml_ui_contract=passed");
  }
  if(options.show_fsr_settings)game_ui->show_fsr_settings();
  if(options.show_inventory || options.show_creative) game_ui->show_inventory(options.show_creative);
  else if(options.show_menu) game_ui->show_pause_menu();
  ::camera camera_settings{};
  camera_init(&camera_settings, CAMERA_PROJECTION_PERSPECTIVE);
  LocalPlayerPose pose{};
  bool player_ready = false;
  float benchmark_yaw{}, benchmark_pitch{};
  unsigned frames = 0;
  DistanceValidation distance_validation;
  double attack_until{};
  uint64_t attack_sequence{};
  int result = 0;
  auto last = SDL_GetTicksNS();
  auto last_complete = last;
  const auto start = last;
  uint64_t benchmark_start{};
  bool benchmark_recording{};
  float previous_profile_ms{};
  std::printf("open_world_start shader=slang backend=slang_rhi radius=%u authority=local_server\n", radius);
  std::fflush(stdout);
  while (controls.running) {
    const auto now = SDL_GetTicksNS();
    const double elapsed = static_cast<double>(now - last) / 1e9;
    last = now;
    frame_profile_sample sample{};
    sample.total_ms = static_cast<float>(elapsed * 1000.0);
    const auto event_start=SDL_GetTicksNS();
    read_world_controls(window, controls, options.benchmark_seconds <= 0 && !qualification);
    sample.misc_ms=frame_profile_elapsed_ms_since(event_start);
    sample.post_submit_tail_ms=previous_profile_ms;
    if (!controls.running) break;
    if(options.validate_temporal) {
      temporal_validation.begin_frame(window,controls.ui,graphics::open_world_renderer_stats(renderer),double(now-start)/1e9);
      graphics::open_world_renderer_set_capture_enabled(renderer,temporal_validation.capture_ready());
    }
    if(options.validate_world_items) {
      const auto prior=graphics::open_world_renderer_stats(renderer);
      item_validation.observe(*game_ui,world_items,pose,player_ready && prior.pending_meshes==0 &&
          prior.columns==(2*radius+1)*(2*radius+1),double(now-start)/1e9);
      if(item_validation.complete())break;
      graphics::open_world_renderer_set_capture_enabled(renderer,item_validation.capture_ready());
    }
    item_actions.update(*game_ui,world_items);
    graphics::open_world_renderer_set_items(renderer,world_items.snapshot());
    const auto requested_radius=static_cast<unsigned>(controls.ui.render_distance);
    if (requested_radius!=radius) {
      radius=requested_radius;
      session.set_radius(radius);
      std::printf("render_distance_changed radius=%u\n",radius);
      std::fflush(stdout);
    }
    if (options.benchmark_seconds > 0) {
      player_control_input_clear(&controls.movement);
      controls.actions.clear();
      if (player_ready) { controls.yaw=benchmark_yaw; controls.pitch=benchmark_pitch; }
      controls.flying=false;
    }
    const auto& move = controls.movement;
    LocalPlayerInput input{move.move_forward != 0, move.move_backward != 0,
                           move.move_left != 0, move.move_right != 0,
                           move.move_up != 0, move.move_down != 0,
                           move.sprint != 0, controls.flying, controls.yaw, controls.pitch};
    const auto sim_start = SDL_GetTicksNS();
    const double motion_seconds=benchmark_recording?std::clamp(double(now-benchmark_start)/1e9-5,0.0,options.benchmark_seconds):0;
    if(options.benchmark_streaming_speed>0 && player_ready) {
      auto view=pose;streaming_benchmark.camera(view,motion_seconds);
      session.set_benchmark_stream_center(int(std::floor(view.x/32)),int(std::floor(view.z/32)));
    }
    if(options.validate_world_items)item_validation.input(input,pose);
    session.update(input, elapsed);
    sample.sim_ms = frame_profile_elapsed_ms_since(sim_start);
    if (!session.running()) {
      std::fprintf(stderr, "Local server stopped: %s\n", session.status().c_str());
      result = 1;
      break;
    }
    LocalPlayerPose view_pose=pose;
    if (session.player_pose(pose)) {
      if (!player_ready) {
        controls.yaw = pose.yaw;
        controls.pitch = pose.pitch;
        controls.flying = pose.flying;
        benchmark_yaw=pose.yaw; benchmark_pitch=pose.pitch;
        player_ready = true;
        std::printf("authoritative_player_ready eye=%.3f,%.3f,%.3f\n", pose.x, pose.y, pose.z);
        std::fflush(stdout);
      }
      view_pose=pose;
      streaming_benchmark.camera(view_pose,motion_seconds);
      const auto cx = static_cast<int32_t>(std::floor(view_pose.x / 32.0f));
      const auto cz = static_cast<int32_t>(std::floor(view_pose.z / 32.0f));
      stream.request(cx, cz, radius);
      graphics::open_world_renderer_set_center(renderer, cx, cz, static_cast<int>(radius));
    }
    const auto world_start = SDL_GetTicksNS();
    if (!graphics::open_world_renderer_stream(renderer, stream)) {
      std::fprintf(stderr, "World upload failed: %s\n", graphics::open_world_renderer_status(renderer));
      result = 1;
      break;
    }
    sample.world_ms = frame_profile_elapsed_ms_since(world_start);
    const float zoom = static_cast<float>(1u << controls.zoom);
    const float fov = 2 * std::atan(std::tan(camera_settings.vertical_field_of_view_radians / 2) / zoom);
    auto camera=player_camera(view_pose,controls,fov,stream,interaction);
    if(options.benchmark_streaming_speed>0){camera.yaw=0;camera.pitch=-.35f;}
    if(options.validate_world_items)item_validation.camera(camera);
    if(options.validate_temporal)temporal_validation.camera(camera);
    if (player_ready) {
      interaction.update(stream,{{camera.x,camera.y,camera.z},camera.yaw,camera.pitch},{pose.x,pose.y,pose.z});
      dispatch_inventory_actions(interaction,*game_ui,controls.actions,
          [&](const presentation::BlockEditIntent& edit) {return session.submit_block_edit(edit);},
          [&](audio::ActionSound sound) {audio::play_action_audio(audio_owner.get(),sound);});
      if (controls.actions.edit_requested()) {attack_until=pose.source_seconds+.5; ++attack_sequence;}
    }
    auto ui = graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(ui, profile.snapshot());
    SDL_GetWindowSizeInPixels(window,&width,&height);
    const auto ui_start=SDL_GetTicksNS();
    const auto resolution_stats=graphics::open_world_renderer_stats(renderer);
    game_ui->set_render_resolution(resolution_stats.render_width,resolution_stats.render_height,
        resolution_stats.display_width,resolution_stats.display_height);
    game_ui->update(ui,graphics::open_world_renderer_ui_tile(renderer,interaction.selected()),width,height);
    sample.ui_ms=frame_profile_elapsed_ms_since(ui_start);
    graphics::open_world_renderer_set_lighting(renderer,lighting.values);
    const auto& settings=controls.ui;
    graphics::open_world_renderer_set_scene(renderer,
        {pose.world_day_fraction,pose.source_seconds,settings.sky_gradient_enabled!=0,
         settings.stars_enabled!=0,settings.sun_enabled!=0,settings.moon_enabled!=0,
         settings.pbr_enabled!=0,settings.pom_enabled!=0,settings.clouds_enabled!=0,
         settings.fog_enabled!=0,lighting.values.fog_distance,settings.upscaler_mode,
         settings.fsr_sharpening!=0,settings.fsr_sharpness,settings.fsr_render_scale,
         settings.fsr_dynamic_resolution!=0,settings.fsr_min_scale,settings.fsr_max_scale,settings.fsr_target_fps});
    auto avatar=player_presentation(pose,controls,camera,attack_until,attack_sequence);
    avatar.visible=player_ready;
    graphics::open_world_renderer_set_player(renderer,avatar);
    graphics::SelectionTarget selection;
    const auto& target=interaction.target();
    if (target.hit) {
      selection.x=target.block.x;selection.y=target.block.y;selection.z=target.block.z;
      const int dx=target.adjacent.x-target.block.x,dy=target.adjacent.y-target.block.y,dz=target.adjacent.z-target.block.z;
      selection.face=dx>0?2u:dx<0?3u:dy>0?4u:dy<0?5u:dz>0?0u:dz<0?1u:6u;
    }
    graphics::open_world_renderer_set_selection(renderer,selection);
    const auto render_start = SDL_GetTicksNS();
    const bool rendered=!(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
    if (rendered) {
      if (!graphics::open_world_renderer_render(renderer, camera)) {
        std::fprintf(stderr, "World frame failed: %s\n", graphics::open_world_renderer_status(renderer));
        result = 1;
        break;
      }
      if(options.validate_world_items)item_validation.frame_rendered(graphics::open_world_renderer_captured(renderer));
    } else {
      SDL_Delay(10);
    }
    sample.render_ms = frame_profile_elapsed_ms_since(render_start);
    const auto completed = SDL_GetTicksNS();
    sample.total_ms = frame_profile_elapsed_ms(last_complete, completed);
    last_complete = completed;
    const auto stats = graphics::open_world_renderer_stats(renderer);
    const auto status = stream.status();
    if(options.validate_temporal) {
      temporal_validation.frame_rendered(stats,game_ui->context(),window,
          player_ready && stats.pending_meshes==0 && stats.columns==(2*radius+1)*(2*radius+1),
          rendered,graphics::open_world_renderer_captured(renderer),double(now-start)/1e9);
      if(temporal_validation.complete())break;
    }
    if (options.benchmark_seconds > 0 && !benchmark_start &&
        stats.columns == (2 * radius + 1) * (2 * radius + 1) && stats.pending_meshes==0) {
      benchmark_start = now;
      std::puts("world_benchmark resident=complete meshes=complete warmup_seconds=5");
      std::fflush(stdout);
    }
    if(benchmark_start && !benchmark_recording && double(now-benchmark_start)/1e9>=5) {
      benchmark_recording=true;profile.restart_measurement();
      std::printf("world_benchmark measurement=start frame=%llu\n",static_cast<unsigned long long>(stats.frames));
      std::fflush(stdout);
    }
    const auto profile_start=SDL_GetTicksNS();
    streaming_benchmark.frame(sample,camera,stats,radius,double(now-start)/1e9,
        !benchmark_recording?"startup":motion_seconds>=options.benchmark_seconds?"settle":"moving");
    profile.frame(window, sample, pose, stats, status.c_str(), session.movement_stats());
    previous_profile_ms=frame_profile_elapsed_ms_since(profile_start);
    if(options.validate_distance_changes && distance_validation.advance(window,controls.ui,radius,stats.columns,
        player_ready && rendered && stats.pending_meshes==0)) break;
    if (player_ready && stats.columns == (2 * radius + 1) * (2 * radius + 1)) ++frames;
    if (options.frame_limit > 0 && frames >= static_cast<unsigned>(options.frame_limit)) break;
    if (benchmark_start && static_cast<double>(now - benchmark_start) / 1e9 >= options.benchmark_seconds + 5) {
      if(options.benchmark_streaming_speed<=0)break;
      if(stats.columns==(2*radius+1)*(2*radius+1) && stats.pending_meshes==0) {
        std::printf("world_stream_benchmark settled=complete center=%d,%d camera=%.6f,%.6f,%.6f quads=%llu gpu_bytes=%llu\n",
            int(std::floor(camera.x/32)),int(std::floor(camera.z/32)),camera.x,camera.y,camera.z,
            static_cast<unsigned long long>(stats.quads),static_cast<unsigned long long>(stats.gpu_bytes));
        break;
      }
      if(static_cast<double>(now-benchmark_start)/1e9>=options.benchmark_seconds+125)
        throw std::runtime_error("Streaming benchmark final residency timed out");
    }
    if ((!player_ready || stats.columns == 0) && now - start > 60000000000ull) {
      std::fprintf(stderr, "World startup timed out: %s; %s\n", session.status().c_str(), status.c_str());
      result = 1;
      break;
    }
  }
  if(options.validate_world_items && !item_validation.complete()) {
    std::fputs("World item qualification ended before the authoritative pickup acknowledgement\n",stderr);result=1;
  }
  if(options.validate_temporal && !temporal_validation.complete()) {
    std::fputs("Temporal qualification ended before mode/resize/history verification\n",stderr);result=1;
  }
  if(options.validate_distance_changes && !distance_validation.complete()) {
    std::fprintf(stderr,"Render distance qualification ended before completing grow/shrink\n");result=1;
  }
  if(!graphics::open_world_renderer_flush(renderer)) {
    std::fprintf(stderr,"World graphics completion failed\n");result=1;
  }
  const auto stats = graphics::open_world_renderer_stats(renderer);
  const auto metrics = profile.snapshot().metrics;
  profile.report_slow_frames();
  std::printf("world_profile average_ms=%.3f low_1pct_fps=%.2f worst_ms=%.3f samples=%llu\n",
              metrics.average.ms, metrics.low_1pct.fps, metrics.worst.ms,
              static_cast<unsigned long long>(metrics.sample_count));
  std::printf("open_world_exit code=%d frames=%u columns=%u quads=%llu gpu_bytes=%llu\n",
              result, frames, stats.columns, static_cast<unsigned long long>(stats.quads),
              static_cast<unsigned long long>(stats.gpu_bytes));
  std::fflush(stdout);
  graphics::open_world_renderer_set_ui_context(renderer,nullptr);
  controls.game_ui=nullptr;
  game_ui.reset();
  renderer_owner.reset();
  session.stop();
  return result;
}
}

int run_open_world(const WorldRunOptions& options) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("Octaryn | Loading world", 1280, 720,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (options.benchmark_hidden?SDL_WINDOW_HIDDEN:0));
  if (!window) {
    std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  if(!SDL_SetWindowMinimumSize(window,640,480)) {
    std::fprintf(stderr,"Window minimum size failed: %s\n",SDL_GetError());
    SDL_DestroyWindow(window);SDL_Quit();return 1;
  }
  int result = 1;
  try { result = run_window(window, options); }
  catch (const std::exception& error) { std::fprintf(stderr, "Open world error: %s\n", error.what()); }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}
}
