#include "FrameLoop.h"

#include "BlockInteraction.h"
#include "EmptyWorldMesh.h"
#include "Environment.h"
#include "EventPump.h"
#include "FrameLogs.h"
#include "FrameRender.h"
#include "HostCommands.h"
#include "Input.h"
#include "Log.h"
#include "PresentationSnapshots.h"
#include "WorldIntents.h"
#include "ChunkView.h"
#include "FlyPlayerController.h"
#include "FrameProfile.h"
#include "FunctionProfile.h"
#include "octaryn_client_render_distance.h"
#include "RuntimeControls.h"
#include "octaryn_client_runtime_settings.h"
#include "octaryn_client_world_mesh_upload.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

namespace octaryn_client_app {
namespace {

using octaryn::client::rendering::block_atlas_default_placeable_block;
using octaryn::client::rendering::block_atlas_top_layer_for_block;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr uint16_t kDefaultInteractionPlaceBlock = 29u;

void log_chunk_view_if_changed(uint64_t frame_index,
                               const chunk_view &view,
                               chunk_view &logged_view) {
  if (chunk_view_equal(&view, &logged_view) != 0 &&
      frame_index % 60u != 0u) {
    return;
  }

  if (g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_chunk_view frame=%" PRIu64
        " origin=(%d,%d) width=%d radius=%d source=render_distance_radius "
        "authority=server\n",
        frame_index, view.origin_x, view.origin_z, view.width, view.width / 2);
    std::fflush(g_log);
  }

  if (!write_chunk_view_intent(view, logged_view, frame_index)) {
    return;
  }

  logged_view = view;
}

bool upload_visible_world_mesh(SDL_GPUDevice *gpu_device,
                               world_mesh_upload_frame &visible_frame,
                               world_mesh_gpu_buffers &mesh_buffers,
                               uint64_t frame_index, const char *source,
                               int &result, bool &running) {
  if (visible_frame.chunks.empty()) {
    release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
    return true;
  }

  function_profile_scope upload_profile_scope(
      "world_mesh_upload", frame_index, source);
  if (!upload_world_mesh_frame(gpu_device, visible_frame, mesh_buffers,
                               frame_index)) {
    result = -6;
    running = false;
    return false;
  }
  return true;
}

} // namespace

int run_frame_loop(SDL_GPUDevice *gpu_device, SDL_Window *window,
                   const octaryn::client::rendering::BlockAtlas &atlas,
                   bool game_modules_disabled,
                   singleplayer_server_session &server_session,
                   frame_pacing &frame_pacing,
                   swapchain_state &swapchain_state,
                   client_shader_pipelines &shader_pipelines,
                   std::vector<presentation_block> &world_snapshot_blocks,
                   std::vector<presentation_block> &world_surface_blocks,
                   server_world_time_state &world_time,
                   block_lookup &world_block_lookup) {
  int result = 0;
  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  runtime_controls runtime_controls{};
  runtime_controls_init(&runtime_controls);
  if (octaryn_client_runtime_settings_load(window, &runtime_controls) == 0) {
    log_line("client_settings_load=failed");
  } else {
    log_line("client_settings_load=0");
  }
  runtime_controls_refresh_menu(&runtime_controls, window,
                                               kWindowWidth, kWindowHeight);
  runtime_controls_sync_relative_mouse(&runtime_controls,
                                                      window);
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
  place_camera_over_snapshot(player.camera, world_surface_blocks);
  octaryn_client_camera_update(&player.camera);

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
  world_mesh_upload_scratch mesh_upload_scratch{
      std::vector<octaryn_client_chunk_mesh_upload_record>(
          kMaxChunkMeshUploadsPerFrame),
      std::vector<uint64_t>(kMaxPackedOpaqueFacesPerFrame),
      std::vector<uint64_t>(kMaxPackedTransparentFacesPerFrame),
      std::vector<uint32_t>(kMaxPackedSpriteVerticesPerFrame),
  };
  world_mesh_gpu_buffers mesh_buffers{};
  world_mesh_upload_frame visible_world_mesh_frame{};
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
  if (server_session.enabled) {
    const chunk_view initial_chunk_view =
        chunk_view_for_camera(player.camera.position[0],
                                             player.camera.position[2],
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
      }
    }
  }

  while (running) {
    const uint64_t frame_start_ticks = SDL_GetTicksNS();
    frame_profile_sample profile_sample{};
    const uint64_t misc_start = frame_start_ticks;
    pointer_motion_debug_state pointer_motion{};
    pointer_click_debug_state pointer_click{};
    poll_events(window, gpu_device, frame_pacing, swapchain_state,
                runtime_controls, keys, world_time_controls, block_selection,
                atlas, game_modules_disabled, pointer_motion, pointer_click,
                running, frame_index + 1u);
    profile_sample.misc_ms +=
        frame_profile_elapsed_ms_since(misc_start);

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
    if (!update_client_player_controller(window, player, input,
                                         frame.timing.delta_seconds)) {
      result = -4;
      running = false;
      break;
    }
    const octaryn_client_camera &camera = player.camera;
    apply_input_to_frame(frame, input, camera);
    const client_block_raycast_hit selection_hit =
        game_modules_disabled
            ? raycast_native_empty_world_interaction(camera, world_block_lookup)
            : raycast_block_interaction(camera, world_block_lookup);
    const chunk_view current_chunk_view = chunk_view_for_camera(
        camera.position[0], camera.position[2],
        runtime_controls.render_distance);
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
    const bool empty_world_local_edit =
        game_modules_disabled && selection_hit.has_hit &&
        ((input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u);
    if (!write_block_interaction_intent(
            frame, input, camera, selection_hit, block_selection.selected_block,
            world_snapshot_blocks, world_block_lookup, game_modules_disabled)) {
      result = -8;
      running = false;
      break;
    }
    bool empty_world_stream_mesh_dirty = false;
    if (!poll_server_stream_presentation(
            server_session, game_modules_disabled, empty_world_mesh_chunk_view,
            frame.timing.frame_index, server_stream_poll, world_time,
            world_snapshot_blocks, world_surface_blocks, world_block_lookup,
            player.camera, empty_world_stream_mesh_dirty, result)) {
      running = false;
      break;
    }
    previous_ticks = current_ticks;
    log_client_tick_input_frame(frame);
    profile_sample.sim_ms =
        frame_profile_elapsed_ms_since(sim_start);

    const uint64_t world_start = SDL_GetTicksNS();
    result = octaryn_client_tick(&frame);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    if (game_modules_disabled) {
      if (server_session.enabled &&
          (empty_world_stream_mesh_dirty || empty_world_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "server_background");
        const server_chunk_stream_file &active_server_stream =
            server_stream_poll.active_server_stream;
        const chunk_view mesh_chunk_view =
            !active_server_stream.columns.empty()
                ? chunk_view_from_server_stream(active_server_stream)
                : current_chunk_view;
        if (!active_server_stream.columns.empty()) {
          build_empty_world_mesh_frame_from_stream(
              active_server_stream, world_block_lookup,
              empty_world_mesh_chunk_view, mesh_upload_frame);
        } else {
          build_empty_world_mesh_frame(current_chunk_view,
                                       empty_world_mesh_chunk_view,
                                       world_block_lookup, mesh_upload_frame);
        }
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        empty_world_mesh_chunk_view = mesh_chunk_view;
        if (!upload_visible_world_mesh(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers, frame.timing.frame_index,
                                       "native_empty_server", result,
                                       running)) {
          break;
        }
      } else if (!server_session.enabled &&
                 (!same_chunk_view(empty_world_mesh_chunk_view,
                                   current_chunk_view) ||
                  empty_world_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "client_native");
        build_empty_world_mesh_frame(current_chunk_view,
                                     empty_world_mesh_chunk_view,
                                     world_block_lookup, mesh_upload_frame);
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        empty_world_mesh_chunk_view = current_chunk_view;
        if (!upload_visible_world_mesh(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers, frame.timing.frame_index,
                                       "native_empty_client", result,
                                       running)) {
          break;
        }
      }
    } else {
      world_mesh_upload_frame mesh_upload_frame{};
      if (!drain_chunk_mesh_uploads(frame.timing.frame_index,
                                    mesh_upload_scratch, mesh_upload_frame)) {
        result = -5;
        running = false;
        break;
      }
      if (!mesh_upload_frame.chunks.empty()) {
        merge_world_mesh_upload_frame(visible_world_mesh_frame,
                                      mesh_upload_frame,
                                      frame.timing.frame_index);
        if (!upload_visible_world_mesh(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers, frame.timing.frame_index,
                                       "game_module", result, running)) {
          break;
        }
      }
    }
    const bool world_mesh_active =
        mesh_buffers.opaque_faces != nullptr &&
        !visible_world_mesh_frame.opaque_faces.empty();
    uint32_t drained_updates = 0u;
    if (!world_mesh_active &&
        !drain_presentation_updates(presentation_blocks, drained_updates)) {
      result = -3;
      running = false;
      break;
    }
    log_live_client_frame(frame.timing.frame_index, input,
                          command_frame_counts(), camera, drained_updates,
                          presentation_blocks);
    profile_sample.world_ms =
        frame_profile_elapsed_ms_since(world_start);

    if (!present_frame(gpu_device, window, atlas, presentation_blocks, camera,
                       selection_hit, block_selection.selected_block,
                       shader_pipelines, mesh_buffers, visible_world_mesh_frame,
                       world_time, runtime_controls, last_profile,
                       frame.timing.frame_index, &profile_sample)) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      }
      result = -2;
      running = false;
      break;
    }
    const uint64_t frame_end_ticks = SDL_GetTicksNS();
    profile_sample.total_ms = frame_profile_elapsed_ms(
        frame_start_ticks, frame_end_ticks);
    frame_profile_finalize_sample(&profile_sample);
    frame_metrics_record(&frame_metrics_state, profile_sample.total_ms,
                                        frame_end_ticks);
    last_profile.sample = profile_sample;
    last_profile.metrics = frame_metrics_snapshot_value(
        &frame_metrics_state, frame_end_ticks);
    log_frame_profile(frame.timing.frame_index, last_profile,
                      runtime_controls.debug_overlay_enabled);

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }
  }

  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  return result;
}

} // namespace octaryn_client_app
