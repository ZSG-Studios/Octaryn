#include "WorldSession.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "Camera.h"
#include "LocalSession.h"
#include "LoadingScreen.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "WorldStream.h"
#include "BlockInteraction.h"
#include "UiData.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "PlayerView.h"
#include "SelectionTarget.h"
#include "ActionAudio.h"
#include "InventoryActions.h"
#include "ActionFeedback.h"
#include "DistanceValidation.h"
#include "WorldItemActions.h"
#include "WorldItemsPresentationBridge.h"
#include "WorldItemsValidation.h"
#include "BlockActionsValidation.h"
#include "TemporalValidation.h"
#include "StreamingBenchmark.h"
#include "LightingMovementValidation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace octaryn::client::app {
namespace graphics = octaryn::client::rendering;
namespace presentation = octaryn::client::world_presentation;

SessionOutcome run_world_session(WorldSession& ctx, LocalSession& session) {
  SDL_Window* window = ctx.window;
  const WorldRunOptions& options = *ctx.options;
  WorldProfile& profile = *ctx.profile;
  StreamingBenchmark& streaming_benchmark = *ctx.streaming;
  LightingPanel& lighting = *ctx.lighting;
  WorldControls& controls = *ctx.controls;
  unsigned& radius = *ctx.radius;
  const std::filesystem::path& world = *ctx.world;
  int& width = *ctx.width;
  int& height = *ctx.height;
  auto* renderer = ctx.renderer;
  auto& interaction = *ctx.interaction;
  audio::ActionAudioOwner& audio_owner = *ctx.audio;
  GameUi* game_ui = ctx.ui;
  bool menu_loading = ctx.show_loading;
  const bool qualification=options.validate_world_items || options.validate_block_actions || options.validate_temporal ||
      options.validate_lighting_motion || options.validate_lighting_edits;
  presentation::WorldStream stream(session.chunk_stream_path());
  presentation::WorldItemsClient world_items(world);
  WorldItemActions item_actions;
  WorldItemsValidation item_validation;
  BlockActionsValidation block_validation;
  if(options.validate_block_actions)graphics::open_world_renderer_set_capture_enabled(renderer,false);
  TemporalValidation temporal_validation;
  LightingMovementValidation lighting_motion;
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
  unsigned ui_frames = 0;
  DistanceValidation distance_validation;
  double attack_until{};
  uint64_t attack_sequence{};
  int result = 0;
  bool disconnect_requested = false;
 std::string receipt_session;
 uint64_t receipt_sequence{};
  auto last = SDL_GetTicksNS();
  auto last_complete = last;
  const auto start = last;
  uint64_t benchmark_start{};
  bool benchmark_recording{};
  float previous_profile_ms{};
  std::printf("open_world_start shader=slang backend=slang_rhi radius=%u authority=%s\n", radius,
      ctx.remote_authority ? "remote_server" : "local_server");
  std::fflush(stdout);
  while (controls.running) {
    const auto now = SDL_GetTicksNS();
    const double elapsed = static_cast<double>(now - last) / 1e9;
    last = now;
    frame_profile_sample sample{};
    sample.total_ms = static_cast<float>(elapsed * 1000.0);
    const auto event_start=SDL_GetTicksNS();
    read_world_controls(window, controls, options.benchmark_seconds <= 0 && !qualification && !options.validate_lighting_motion);
    sample.misc_ms=frame_profile_elapsed_ms_since(event_start);
    sample.post_submit_tail_ms=previous_profile_ms;
    if (!controls.running) break;
    if (controls.ui.display_menu.action_requested == DISPLAY_MENU_ACTION_DISCONNECT_SESSION) {
      controls.ui.display_menu.action_requested = DISPLAY_MENU_ACTION_NONE;
      disconnect_requested = true;
      break;
    }
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
    const auto requested_radius=static_cast<unsigned>(controls.ui.render_distance);
    if (requested_radius!=radius) {
      radius=requested_radius;
      session.set_radius(radius);
      std::printf("render_distance_changed radius=%u\n",radius);
      std::fflush(stdout);
    }
    if (options.benchmark_seconds > 0 || options.validate_lighting_motion || options.validate_lighting_edits) {
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
    take_jump_input(controls, input);
    const auto sim_start = SDL_GetTicksNS();
    const double motion_seconds=benchmark_recording?std::clamp(double(now-benchmark_start)/1e9-5,0.0,options.benchmark_seconds):0;
    if(options.benchmark_streaming_speed>0 && player_ready) {
      auto view=pose;streaming_benchmark.camera(view,motion_seconds);
      session.set_benchmark_stream_center(int(std::floor(view.x/32)),int(std::floor(view.z/32)));
    }
    if(options.validate_world_items)item_validation.input(input,pose);
    if(options.validate_block_actions)block_validation.input(input);
    session.update(input, elapsed);
    const auto& receipts=session.block_receipts();
    if (!receipts.session.empty() && receipts.session!=receipt_session) {
      if (!receipt_session.empty()) {
        stream.reset_predictions();
        graphics::open_world_renderer_reset_predictions(renderer);
      }
      receipt_session=receipts.session; receipt_sequence=0;
    }
    for (const auto& receipt:receipts.receipts) {
      if (receipt.sequence<=receipt_sequence) continue;
      stream.resolve_block(receipt.commandID,receipt.accepted,receipt.revision);
      graphics::open_world_renderer_resolve_predicted_edit(renderer,receipt.commandID,receipt.accepted,receipt.revision);
      if(options.validate_block_actions)block_validation.receipt(receipt);
      receipt_sequence=receipt.sequence;
    }
    if (receipt_sequence) session.acknowledge_block_receipts(receipt_session,receipt_sequence);
    LocalPlayerPose toss_eye=pose;
    const bool toss_ready=session.player_pose(toss_eye);
    const presentation::TossPose toss_pose{toss_eye.x,toss_eye.y,toss_eye.z,toss_eye.yaw,toss_eye.pitch};
    item_actions.update(*game_ui,world_items,toss_ready?&toss_pose:nullptr);
    graphics::set_world_items_presentation(renderer,world_items.presentation());
    if (controls.time_hour_steps) session.step_world_hours(controls.time_hour_steps);
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
    if(options.validate_block_actions)block_validation.camera(camera);
    if(options.validate_temporal)temporal_validation.camera(camera);
    if(options.validate_lighting_motion)
      graphics::open_world_renderer_set_capture_enabled(renderer,
          lighting_motion.camera(camera,graphics::open_world_renderer_stats(renderer),radius));
    if (player_ready) {
      interaction.update(stream,{{camera.x,camera.y,camera.z},camera.yaw,camera.pitch},{pose.x,pose.y,pose.z});
      const auto submit_edit=[&](const presentation::BlockEditIntent& edit,uint64_t* issued) {
            if(!stream.can_predict() || !graphics::open_world_renderer_can_predict(renderer))return false;
 uint64_t command{};
 if(!session.submit_block_edit(edit,&command))return false;
 const bool query_predicted=stream.predict_block(command,edit.edit.x,edit.edit.y,edit.edit.z,edit.block);
 const bool render_predicted=graphics::open_world_renderer_apply_predicted_edit(renderer,command,
 edit.edit.x,edit.edit.y,edit.edit.z,edit.block);
 if(options.validate_block_actions&&(!query_predicted||!render_predicted))
   throw std::runtime_error("Block qualification prediction was not applied");
 if(issued)*issued=command;
 return true;
      };
      if(options.validate_block_actions) {
        const auto stats=graphics::open_world_renderer_stats(renderer);
        block_validation.update(stream,interaction,pose,stats.pending_meshes==0&&
          stats.columns==(2*radius+1)*(2*radius+1),double(now-start)/1e9,submit_edit);
        graphics::open_world_renderer_set_capture_enabled(renderer,block_validation.capture_ready());
      } else dispatch_inventory_actions(interaction,*game_ui,controls.actions,
          [&](const presentation::BlockEditIntent& edit){return submit_edit(edit,nullptr);},
          [&](audio::ActionSound sound) {audio::play_action_audio(audio_owner.get(),sound);});
    if (controls.actions.edit_requested()) {attack_until=double(now-start)/1e9+.5; ++attack_sequence;}
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
    graphics::open_world_renderer_set_lighting_debug(renderer,lighting.debug_view);
    graphics::open_world_renderer_set_lighting_quality(renderer,controls.ui.lighting_quality);
    graphics::open_world_renderer_set_raster_shadows(renderer,controls.ui.raster_sun_shadows);
    graphics::open_world_renderer_set_trace_ranges(renderer,float(controls.ui.shadow_distance),
        float(controls.ui.reflection_distance));
    graphics::open_world_renderer_set_present(renderer,controls.ui.present_mode_index);
    const auto& settings=controls.ui;
    graphics::open_world_renderer_set_scene(renderer,
        {pose.world_day_fraction,pose.source_seconds,settings.sky_gradient_enabled!=0,
         settings.stars_enabled!=0,settings.sun_enabled!=0,settings.moon_enabled!=0,
         settings.pbr_enabled!=0,settings.pom_enabled!=0,settings.clouds_enabled!=0,
         settings.fog_enabled!=0,lighting.values.fog_distance,settings.upscaler_mode,
         settings.fsr_sharpening!=0,settings.fsr_sharpness,settings.fsr_render_scale,
         settings.fsr_dynamic_resolution!=0,settings.fsr_min_scale,settings.fsr_max_scale,settings.fsr_target_fps,settings.ray_tracing_enabled!=0});
    if(!graphics::open_world_renderer_set_ddgi_range(renderer,controls.ui.gi_voxel_radius,
        controls.ui.gi_coarse_radius)) {
      std::fprintf(stderr,"GI range apply failed: %s\n",graphics::open_world_renderer_status(renderer));
      result=1;break;
    }
    auto avatar=player_presentation(pose,controls,camera,double(now-start)/1e9,attack_until,attack_sequence);
    avatar.visible=player_ready;
    graphics::open_world_renderer_set_player(renderer,avatar);
    graphics::SelectionTarget selection;
    const auto& target=interaction.target();
    if (target.hit) {
      selection.x=target.block.x;selection.y=target.block.y;selection.z=target.block.z;
      const int dx=target.adjacent.x-target.block.x,dy=target.adjacent.y-target.block.y,dz=target.adjacent.z-target.block.z;
      selection.face=dx>0?2u:dx<0?3u:dy>0?4u:dy<0?5u:dz>0?0u:dz<0?1u:6u;
      selection.opening=controls.breaking && target.actionable?1u:0u;
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
      if(options.validate_block_actions)block_validation.frame_rendered();
    } else {
      SDL_Delay(10);
    }
    sample.render_ms = frame_profile_elapsed_ms_since(render_start);
    ++ui_frames;
    if(options.validate_block_actions&&block_validation.complete())break;
    const bool cli_capture=!options.capture_ui.empty() && ui_frames==5;
    if (cli_capture || game_ui->consume_ui_capture_request()) {
      char* directory=SDL_GetPrefPath("ZSGStudios","Octaryn");
      if (directory) {
        auto folder=std::filesystem::path(reinterpret_cast<const char8_t*>(directory))/"ui-captures";
        SDL_free(directory);
        std::error_code error;std::filesystem::create_directories(folder,error);
        const std::string name=cli_capture?options.capture_ui:"ui-"+std::to_string(SDL_GetTicksNS()/1000000ull);
        const auto path=folder/(name+".bmp");
        const auto utf8=path.generic_u8string();
        if (!graphics::open_world_renderer_capture_ui(renderer,reinterpret_cast<const char*>(utf8.c_str())))
          std::fprintf(stderr,"UI capture failed\n");
      }
    }
    const auto completed = SDL_GetTicksNS();
    sample.total_ms = frame_profile_elapsed_ms(last_complete, completed);
    last_complete = completed;
    if(!options.frame_limit && options.benchmark_seconds<=0) {
      unsigned cap=settings.frame_cap_fps;
      if(cap==1) {
        const auto* mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
        cap=mode && mode->refresh_rate>1.f?static_cast<unsigned>(std::lround(mode->refresh_rate)):0;
      }
      if(cap>0) {
        const std::uint64_t period=1'000'000'000ull/cap;
        const std::uint64_t now_ns=SDL_GetTicksNS();
        if(now_ns<last_complete+period) SDL_DelayNS(last_complete+period-now_ns);
      }
    }
    const auto stats = graphics::open_world_renderer_stats(renderer);
        const auto status = stream.status();
        if (options.validate_session_rejoin) {
            if (player_ready && !menu_loading && stats.columns == (2 * radius + 1) * (2 * radius + 1)
                && stats.pending_meshes == 0 && ui_frames >= 360) {
                controls.ui.display_menu.action_requested = DISPLAY_MENU_ACTION_DISCONNECT_SESSION;
            }
            if (double(now - start) / 1e9 > 60) {
                std::fprintf(stderr, "Session rejoin qualification timed out waiting for world presentation\n");
                result = 1;
                break;
            }
        }
    if (menu_loading && update_world_loading(*game_ui, controls.ui, window,
        player_ready, stats.columns, radius, stats.pending_meshes, session.status()))
      menu_loading = false;
    if(options.validate_temporal) {
      temporal_validation.frame_rendered(stats,game_ui->context(),window,
          player_ready && stats.pending_meshes==0 && stats.columns==(2*radius+1)*(2*radius+1),
          rendered,graphics::open_world_renderer_captured(renderer),double(now-start)/1e9);
      if(temporal_validation.complete())break;
    }
    if (options.benchmark_seconds > 0 && !benchmark_start &&
        stats.columns == (2 * radius + 1) * (2 * radius + 1) && stats.pending_meshes==0 && stats.ray_pending_columns==0) {
      benchmark_start = now;
      std::puts("world_benchmark resident=complete meshes=complete warmup_seconds=5");
      std::printf("world_benchmark ray_tracing=%u ready=%u pending=%u\n",stats.ray_tracing_active?1u:0u,stats.ray_ready_columns,stats.ray_pending_columns);
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
      if(stats.columns==(2*radius+1)*(2*radius+1) && stats.pending_meshes==0 && stats.ray_pending_columns==0) {
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
  if(options.validate_block_actions&&!block_validation.complete()) {
    std::fputs("Block action qualification ended before all seven captures\n",stderr);result=1;
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
  graphics::open_world_renderer_reset_predictions(renderer);
  stream.reset_predictions();
  graphics::set_world_items_presentation(renderer,{});
  return SessionOutcome{disconnect_requested, result};
}
}
