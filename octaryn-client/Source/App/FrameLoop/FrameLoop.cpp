#include "FrameLoop.h"

#include "BlockInteraction.h"
#include "ChunkView.h"
#include "EmptyWorldMesh.h"
#include "Environment.h"
#include "EventPump.h"
#include "FlyPlayerController.h"
#include "FrameLogs.h"
#include "FrameLoopSupport.h"
#include "FrameProfile.h"
#include "FrameRender.h"
#include "HostCommands.h"
#include "Input.h"
#include "Log.h"
#include "MenuWorldActions.h"
#include "PlayerModelPass.h"
#include "PresentationSnapshots.h"
#include "RenderDistance.h"
#include "RuntimeControls.h"
#include "RuntimeSettings.h"
#include "SessionRuntimeReset.h"
#include "WorldIntents.h"
#include "WorldMeshRuntime.h"
#include "WorldMeshUpload.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

namespace octaryn_client_app {
namespace {

constexpr int kMainMenuFpsCap = 60;

using octaryn::client::rendering::block_atlas_default_placeable_block;
using octaryn::client::rendering::block_atlas_top_layer_for_block;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr uint16_t kDefaultInteractionPlaceBlock = 29u;

void log_chunk_view_if_changed(uint64_t frame_index, const chunk_view &view,
                               chunk_view &logged_view) {
  const bool should_log =
      chunk_view_equal(&view, &logged_view) == 0 || frame_index % 30u == 0u;
  if (should_log && g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_chunk_view frame=%" PRIu64
        " origin=(%d,%d) width=%d radius=%d source=render_distance_radius "
        "authority=server\n",
        frame_index, view.origin_x, view.origin_z, view.width, view.width / 2);
    std::fflush(g_log);
  }

  if (!chunk_view_intent_needs_progress(view)) {
    return;
  }

  if (!write_chunk_view_intent(view, logged_view, frame_index)) {
    return;
  }

  if (should_log) {
    logged_view = view;
  }
}

bool main_menu_fps_cap_active(const runtime_controls &controls) {
  return controls.session_active == 0u && controls.display_menu.active != 0u;
}

} // namespace

int run_frame_loop(SDL_GPUDevice *gpu_device, SDL_Window *window,
                   const octaryn::client::rendering::BlockAtlas &atlas,
                   bool game_modules_disabled,
                   singleplayer_server_session &server_session,
                   frame_pacing &frame_pacing, swapchain_state &swapchain_state,
                   client_shader_pipelines &shader_pipelines,
                   std::vector<presentation_block> &world_snapshot_blocks,
                   std::vector<presentation_block> &world_surface_blocks,
                   server_world_time_state &world_time,
                   block_lookup &world_block_lookup) {
  int result = 0;
  const uint32_t exit_after_frames = read_exit_after_frames();
  const double exit_after_seconds = read_exit_after_seconds();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t first_frame_start_ticks = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  runtime_controls runtime_controls{};
  runtime_controls_init(&runtime_controls);
  if (runtime_settings_load(window, &runtime_controls) == 0) {
    log_line("client_settings_load=failed");
  } else {
    log_line("client_settings_load=0");
  }
  apply_movement_probe_render_distance(runtime_controls);
  runtime_controls_refresh_menu(&runtime_controls, window, kWindowWidth,
                                kWindowHeight);
  runtime_controls_sync_relative_mouse(&runtime_controls, window);
  frame_pacing.requested_present_mode =
      runtime_controls.present_mode_index == 0
          ? PRESENT_MODE_POLICY_IMMEDIATE
          : (runtime_controls.present_mode_index == 1
                 ? PRESENT_MODE_POLICY_MAILBOX
                 : PRESENT_MODE_POLICY_VSYNC);
  if (swapchain_configure(&swapchain_state, gpu_device, window,
                          &frame_pacing) &&
      g_log != nullptr) {
    std::fprintf(g_log,
                 "gpu_swapchain_configure=0 source=settings present_mode=%s "
                 "fps_cap=%d\n",
                 swapchain_present_mode_name(&swapchain_state),
                 frame_pacing.fps_cap);
    std::fflush(g_log);
  }
  frame_metrics frame_metrics_state{};
  frame_metrics_init(&frame_metrics_state);
  frame_profile_snapshot last_profile{};
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_runtime_controls active=1 f3=1 f11=1 escape_menu=1 "
                 "menu=%u debug=%u render_distance=%d\n",
                 static_cast<unsigned>(runtime_controls.display_menu.active),
                 static_cast<unsigned>(runtime_controls.debug_overlay_enabled),
                 runtime_controls.render_distance);
    std::fflush(g_log);
  }

  fly_player_controller player{};
  fly_player_controller_init(&player);
  apply_render_distance_far_plane(player.camera,
                                  runtime_controls.render_distance);
  camera_update(&player.camera);
  place_camera_over_snapshot(player.camera, world_surface_blocks);
  apply_movement_probe_camera_spawn(player.camera);
  camera_update(&player.camera);

  block_selection_state block_selection{};
  if (!game_modules_disabled) {
    block_selection.selected_block = block_atlas_default_placeable_block(
        atlas, kDefaultInteractionPlaceBlock);
  } else {
    block_selection.selected_block = 1u;
  }
  if (g_log != nullptr && !game_modules_disabled) {
    const int32_t layer =
        block_atlas_top_layer_for_block(atlas, block_selection.selected_block);
    std::fprintf(
        g_log,
        "live_selected_block block=%u layer=%d source=game_module_catalog\n",
        static_cast<unsigned>(block_selection.selected_block), layer);
    std::fflush(g_log);
  } else if (g_log != nullptr) {
    log_line("live_selected_block active=0 reason=game_modules_disabled");
  }

  std::vector<presentation_block> presentation_blocks;
  world_mesh_gpu_buffers mesh_buffers{};
  world_mesh_runtime mesh_runtime{};
  if (!world_mesh_runtime_start(mesh_runtime)) {
    result = -10;
    running = false;
  }
  world_mesh_upload_frame visible_world_mesh_frame{};
  frame_render_targets render_targets{};
  chunk_view empty_world_mesh_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  client_key_state keys{};
  client_world_time_controls world_time_controls{};
  chunk_view logged_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  client_server_stream_poll_state server_stream_poll{};
  server_stream_poll.active_server_stream_override_signature =
      std::numeric_limits<uint64_t>::max();
  if (!server_session.enabled) {
    server_chunk_stream_file initial_stream{};
    if (load_server_chunk_stream_file(initial_stream, world_time, true) &&
        !initial_stream.columns.empty()) {
      server_stream_poll.active_server_stream_override_signature =
          hash_world_block_records(initial_stream.blocks);
      server_stream_poll.active_server_stream = std::move(initial_stream);
    }
  }
  if (server_session.enabled) {
    const chunk_view initial_chunk_view = chunk_view_for_camera(
        player.camera.position[0], player.camera.position[2],
        runtime_controls.render_distance);
    chunk_view empty_previous_view{
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::min(),
        0,
    };
    if (!write_chunk_view_intent(initial_chunk_view, empty_previous_view, 1u)) {
      result = -9;
      running = false;
    } else {
      octaryn_host_frame_snapshot initial_frame =
          create_frame(0u, kDefaultDeltaSeconds);
      client_input_debug_state initial_input{};
      apply_input_to_frame(initial_frame, initial_input, player.camera);
      if (!write_player_input_intent(initial_frame) ||
          !write_world_time_intent(server_session, world_time_controls) ||
          !start_singleplayer_server(server_session)) {
        result = -9;
        running = false;
      } else {
        runtime_controls.session_active = 1u;
        display_menu_close(&runtime_controls.display_menu);
        runtime_controls_sync_relative_mouse(&runtime_controls, window);
      }
    }
  }
  while (running) {
    const uint64_t frame_start_ticks = SDL_GetTicksNS();
    if (first_frame_start_ticks == 0u) {
      first_frame_start_ticks = frame_start_ticks;
    }
    frame_profile_sample profile_sample{};
    const uint64_t misc_start = frame_start_ticks;
    pointer_motion_debug_state pointer_motion{};
    pointer_click_debug_state pointer_click{};
    uint32_t menu_action = DISPLAY_MENU_ACTION_NONE;
    uint32_t menu_world_slot = 0u;
    poll_events(window, gpu_device, frame_pacing, swapchain_state,
                runtime_controls, keys, world_time_controls, block_selection,
                atlas, game_modules_disabled, pointer_motion, pointer_click,
                running, menu_action, menu_world_slot, frame_index + 1u);
    if (runtime_controls.display_menu.active != 0u &&
        runtime_controls.display_menu.screen ==
            DISPLAY_MENU_SCREEN_SINGLEPLAYER) {
      refresh_singleplayer_world_slots(runtime_controls);
      if (display_menu_row_selectable(&runtime_controls.display_menu,
                                      runtime_controls.display_menu.row) ==
          0u) {
        display_menu_move_row(&runtime_controls.display_menu, 1);
      }
    }
    uint32_t menu_status_code = runtime_controls.display_menu.status_code;
    const menu_action_result menu_result = run_menu_action(
        server_session, game_modules_disabled, player.camera,
        runtime_controls.render_distance, world_time_controls, menu_action,
        menu_world_slot, runtime_controls.display_menu.server_address,
        runtime_controls.display_menu.server_port,
        runtime_controls.display_menu.world_name, menu_status_code, result);
    runtime_controls.display_menu.status_code = menu_status_code;
    if (menu_result == MENU_ACTION_RESULT_FATAL) {
      running = false;
      break;
    }
    if (menu_result == MENU_ACTION_RESULT_COMPLETED &&
        (menu_action == DISPLAY_MENU_ACTION_LOAD_WORLD ||
         menu_action == DISPLAY_MENU_ACTION_CREATE_WORLD ||
         menu_action == DISPLAY_MENU_ACTION_CONNECT_SERVER ||
         menu_action == DISPLAY_MENU_ACTION_CONNECT_LOCAL)) {
      runtime_controls.session_active = 1u;
      display_menu_close(&runtime_controls.display_menu);
      runtime_controls_sync_relative_mouse(&runtime_controls, window);
    }
    if (menu_result == MENU_ACTION_RESULT_COMPLETED &&
        menu_action == DISPLAY_MENU_ACTION_DISCONNECT_SESSION) {
      if (!reset_session_runtime_state(
              gpu_device, mesh_buffers, mesh_runtime, visible_world_mesh_frame,
              server_stream_poll, presentation_blocks, world_snapshot_blocks,
              world_surface_blocks, world_block_lookup,
              empty_world_mesh_chunk_view, logged_chunk_view, world_time,
              result)) {
        running = false;
        break;
      }
      runtime_controls.session_active = 0u;
      runtime_controls.display_menu.active = 1u;
      runtime_controls.display_menu.screen = DISPLAY_MENU_SCREEN_MAIN;
      runtime_controls.display_menu.row = 2;
      runtime_controls_sync_relative_mouse(&runtime_controls, window);
    }
    apply_movement_probe_render_distance(runtime_controls);
    profile_sample.misc_ms += frame_profile_elapsed_ms_since(misc_start);

    const uint64_t current_ticks = SDL_GetTicksNS();
    const uint64_t sim_start = current_ticks;
    double delta_seconds = frame_delta_seconds(previous_ticks, current_ticks);
    if (read_enabled_flag(kInputProbeFlag) && frame_index == 0u) {
      delta_seconds = kDefaultDeltaSeconds;
    }
    octaryn_host_frame_snapshot frame =
        create_frame(frame_index + 1u, delta_seconds);
    client_input_debug_state input =
        read_client_input(window, pointer_motion, pointer_click, keys);
    if (runtime_controls_ui_active(&runtime_controls) != 0u) {
      input = {};
    }
    apply_input_probe(input, frame.timing.frame_index);
    apply_render_distance_far_plane(player.camera,
                                    runtime_controls.render_distance);
    const uint64_t controller_start = SDL_GetTicksNS();
    if (!update_client_player_controller(window, player, input,
                                         frame.timing.delta_seconds)) {
      result = -4;
      running = false;
      break;
    }
    const float controller_ms =
        frame_profile_elapsed_ms_since(controller_start);
    const uint64_t terrain_align_start = SDL_GetTicksNS();
    align_movement_probe_camera_to_terrain(player.camera,
                                           frame.timing.frame_index);
    const float terrain_align_ms =
        frame_profile_elapsed_ms_since(terrain_align_start);
    const camera &camera = player.camera;
    apply_input_to_frame(frame, input, camera);
    const uint64_t raycast_start = SDL_GetTicksNS();
    const client_block_raycast_hit selection_hit =
        (game_modules_disabled || server_session.enabled)
            ? raycast_native_empty_world_interaction(camera, world_block_lookup)
            : raycast_block_interaction(camera, world_block_lookup);
    const float raycast_ms = frame_profile_elapsed_ms_since(raycast_start);
    chunk_view current_chunk_view =
        chunk_view_for_camera(camera.position[0], camera.position[2],
                              runtime_controls.render_distance);
    const uint64_t intent_start = SDL_GetTicksNS();
    log_chunk_view_if_changed(frame.timing.frame_index, current_chunk_view,
                              logged_chunk_view);
    reset_command_frame_counts();
    if (!write_player_input_intent(frame)) {
      result = -7;
      running = false;
      break;
    }
    if (world_time_controls.dirty) {
      if (!write_world_time_intent(server_session, world_time_controls)) {
        result = -9;
        running = false;
        break;
      }
      world_time_controls.dirty = false;
    }
    const bool had_server_stream_before_poll =
        !server_stream_poll.active_server_stream.columns.empty();
    const bool preserve_seed_air_edits = game_modules_disabled ||
                                         server_session.enabled ||
                                         had_server_stream_before_poll;
    const bool empty_world_local_edit =
        preserve_seed_air_edits && selection_hit.has_hit &&
        ((input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u);
    std::vector<empty_world_dirty_column> mesh_dirty_columns =
        server_stream_poll.active_server_stream_dirty_columns;
    if (!write_block_interaction_intent(
            frame, input, camera, selection_hit, block_selection.selected_block,
            world_snapshot_blocks, world_block_lookup,
            preserve_seed_air_edits, !server_session.enabled,
            mesh_dirty_columns)) {
      result = -8;
      running = false;
      break;
    }
    const float intent_ms = frame_profile_elapsed_ms_since(intent_start);
    advance_server_world_time(world_time, frame.timing.delta_seconds,
                              world_time_controls.speed_multiplier);
    bool empty_world_stream_mesh_dirty = false;
    const uint64_t poll_stream_start = SDL_GetTicksNS();
    if (!poll_server_stream_presentation(
            server_session, game_modules_disabled, empty_world_mesh_chunk_view,
            frame.timing.frame_index, server_stream_poll, world_time,
            world_snapshot_blocks, world_surface_blocks, world_block_lookup,
            player.camera, empty_world_stream_mesh_dirty, result)) {
      running = false;
      break;
    }
    const float poll_stream_ms =
        frame_profile_elapsed_ms_since(poll_stream_start);
    current_chunk_view = chunk_view_for_camera(
        player.camera.position[0], player.camera.position[2],
        runtime_controls.render_distance);
    log_camera_terrain_state(player.camera, world_block_lookup,
                             frame.timing.frame_index);
    const bool has_server_stream =
        !server_stream_poll.active_server_stream.columns.empty();
    previous_ticks = current_ticks;
    log_client_tick_input_frame(frame);
    profile_sample.sim_ms = frame_profile_elapsed_ms_since(sim_start);

    const uint64_t world_start = SDL_GetTicksNS();
    const uint64_t host_tick_start = SDL_GetTicksNS();
    result = octaryn_client_tick(&frame);
    const float host_tick_ms = frame_profile_elapsed_ms_since(host_tick_start);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    const uint64_t mesh_update_start = SDL_GetTicksNS();
    if (!run_frame_world_mesh_update(
            mesh_runtime, gpu_device, visible_world_mesh_frame, mesh_buffers,
            game_modules_disabled, server_session.enabled, has_server_stream,
            empty_world_stream_mesh_dirty,
            empty_world_local_edit, server_stream_poll.active_server_stream,
            mesh_dirty_columns,
            current_chunk_view, empty_world_mesh_chunk_view, world_block_lookup,
            frame.timing.frame_index, result)) {
      running = false;
      break;
    }
    const float mesh_update_ms =
        frame_profile_elapsed_ms_since(mesh_update_start);

    const bool world_mesh_active = world_mesh_gpu_has_geometry(mesh_buffers);
    uint32_t drained_updates = 0u;
    const uint64_t presentation_start = SDL_GetTicksNS();
    if (!world_mesh_active &&
        !drain_presentation_updates(presentation_blocks, drained_updates)) {
      result = -3;
      running = false;
      break;
    }
    const float presentation_ms =
        frame_profile_elapsed_ms_since(presentation_start);
    log_live_client_frame(frame.timing.frame_index, input,
                          command_frame_counts(), camera, drained_updates,
                          presentation_blocks);
    log_frame_phase_profile(frame.timing.frame_index, controller_ms,
                            terrain_align_ms, raycast_ms, intent_ms,
                            poll_stream_ms, host_tick_ms, mesh_update_ms,
                            presentation_ms);
    profile_sample.world_ms = frame_profile_elapsed_ms_since(world_start);

    const struct camera render_camera =
        build_player_render_camera(camera, runtime_controls);
    if (!present_frame(gpu_device, window, atlas, presentation_blocks, camera,
                       render_camera, selection_hit,
                       block_selection.selected_block, input, shader_pipelines,
                       render_targets, mesh_buffers, visible_world_mesh_frame,
                       world_time, runtime_controls, last_profile,
                       frame.timing.frame_index, &profile_sample)) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      }
      result = -2;
      running = false;
      break;
    }
    profile_sample.fps_cap_sleep_ms =
        main_menu_fps_cap_active(runtime_controls)
            ? frame_pacing_sleep_until_frame_cap(
                  &frame_pacing, frame_start_ticks, kMainMenuFpsCap)
            : frame_pacing_sleep_until_next_frame(&frame_pacing,
                                                  frame_start_ticks);
    const uint64_t frame_end_ticks = SDL_GetTicksNS();
    profile_sample.total_ms =
        frame_profile_elapsed_ms(frame_start_ticks, frame_end_ticks);
    frame_profile_finalize_sample(&profile_sample);
    frame_metrics_record(&frame_metrics_state, profile_sample.total_ms,
                         frame_end_ticks);
    last_profile.sample = profile_sample;
    last_profile.metrics =
        frame_metrics_snapshot_value(&frame_metrics_state, frame_end_ticks);
    log_frame_profile(frame.timing.frame_index, last_profile,
                      runtime_controls.debug_overlay_enabled);

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }
    if (exit_after_seconds > 0.0 &&
        frame_profile_elapsed_ms(first_frame_start_ticks, frame_end_ticks) >=
            static_cast<float>(exit_after_seconds * 1000.0)) {
      running = false;
    }
  }

  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  world_mesh_runtime_stop(mesh_runtime);
  release_frame_render_targets(gpu_device, render_targets);
  return result;
}

} // namespace octaryn_client_app
