#include "octaryn_client_app_block_interaction.h"
#include "octaryn_client_app_environment.h"
#include "octaryn_client_app_event_pump.h"
#include "octaryn_client_app_file_io.h"
#include "octaryn_client_app_frame_logs.h"
#include "octaryn_client_app_frame_render.h"
#include "octaryn_client_app_host_commands.h"
#include "octaryn_client_app_input.h"
#include "octaryn_client_app_json_files.h"
#include "octaryn_client_app_log.h"
#include "octaryn_client_app_native_empty_atlas.h"
#include "octaryn_client_app_presentation_snapshots.h"
#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_app_shader_pipelines.h"
#include "octaryn_client_app_window.h"
#include "octaryn_client_app_world_intents.h"
#include "octaryn_client_app_world_stream.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_chunk_view.h"
#include "octaryn_client_fly_player_controller.h"
#include "octaryn_client_frame_profile.h"
#include "octaryn_client_function_profile.h"
#include "octaryn_client_host_exports.h"
#include "octaryn_client_native_empty_world_mesh.h"
#include "octaryn_client_render_distance.h"
#include "octaryn_client_runtime_controls.h"
#include "octaryn_client_runtime_settings.h"
#include "octaryn_client_swapchain.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_client_world_mesh_upload.h"
#include "octaryn_native_crash_diagnostics.h"
#include "octaryn_singleplayer_server_session.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using octaryn::client::rendering::client_block_atlas_default_placeable_block;
using octaryn::client::rendering::client_block_atlas_top_layer_for_block;
using octaryn::client::rendering::ClientBlockAtlas;
using octaryn::client::rendering::load_client_block_atlas;
using octaryn_client_app::apply_input_probe;
using octaryn_client_app::apply_input_to_frame;
using octaryn_client_app::apply_snapshot_blocks;
using octaryn_client_app::block_selection_state;
using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::build_client_bundle_path;
using octaryn_client_app::client_block_raycast_hit;
using octaryn_client_app::client_chunk_view_intent_file;
using octaryn_client_app::client_command_frame_counts;
using octaryn_client_app::client_input_debug_state;
using octaryn_client_app::client_key_state;
using octaryn_client_app::client_player_input_intent_file;
using octaryn_client_app::client_server_stream_poll_state;
using octaryn_client_app::client_shader_pipelines;
using octaryn_client_app::client_world_time_controls;
using octaryn_client_app::client_world_time_intent_file;
using octaryn_client_app::close_log;
using octaryn_client_app::command_frame_counts;
using octaryn_client_app::create_frame;
using octaryn_client_app::drain_presentation_updates;
using octaryn_client_app::enqueue_command;
using octaryn_client_app::frame_delta_seconds;
using octaryn_client_app::g_log;
using octaryn_client_app::has_block_override;
using octaryn_client_app::initialize_shader_pipelines;
using octaryn_client_app::kInputPrimaryFlag;
using octaryn_client_app::kInputProbeFlag;
using octaryn_client_app::kInputSecondaryFlag;
using octaryn_client_app::kInputSprintFlag;
using octaryn_client_app::load_native_empty_world_atlas;
using octaryn_client_app::load_world_snapshot_blocks;
using octaryn_client_app::log_client_tick_input_frame;
using octaryn_client_app::log_frame_profile;
using octaryn_client_app::log_line;
using octaryn_client_app::log_live_client_frame;
using octaryn_client_app::log_result;
using octaryn_client_app::open_log;
using octaryn_client_app::pack_block;
using octaryn_client_app::pack_signed_pair;
using octaryn_client_app::place_camera_over_snapshot;
using octaryn_client_app::pointer_click_debug_state;
using octaryn_client_app::pointer_motion_debug_state;
using octaryn_client_app::poll_client_app_events;
using octaryn_client_app::poll_server_stream_presentation;
using octaryn_client_app::prepare_singleplayer_server_session;
using octaryn_client_app::present_frame;
using octaryn_client_app::presentation_block;
using octaryn_client_app::raycast_block_interaction;
using octaryn_client_app::raycast_native_empty_world_interaction;
using octaryn_client_app::read_client_input;
using octaryn_client_app::read_enabled_flag;
using octaryn_client_app::read_exit_after_frames;
using octaryn_client_app::read_text_file;
using octaryn_client_app::release_shader_pipelines;
using octaryn_client_app::reset_command_frame_counts;
using octaryn_client_app::server_chunk_stream_file;
using octaryn_client_app::server_world_time_state;
using octaryn_client_app::singleplayer_server_session;
using octaryn_client_app::start_singleplayer_server;
using octaryn_client_app::stop_singleplayer_server;
using octaryn_client_app::update_client_player_controller;
using octaryn_client_app::write_block_interaction_intent;
using octaryn_client_app::write_chunk_view_intent;
using octaryn_client_app::write_player_input_intent;
using octaryn_client_app::write_world_time_intent;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr const char *kDisableGameModulesFlag =
    "OCTARYN_CLIENT_DISABLE_GAME_MODULES";
constexpr uint16_t kDefaultInteractionPlaceBlock = 29u;

int apply_probe_snapshot() {
  octaryn_replication_change changes[1]{};
  changes[0].version = 1u;
  changes[0].size = OCTARYN_REPLICATION_CHANGE_SIZE;
  changes[0].change_kind = 1u;
  changes[0].replication_id = 1u;
  changes[0].payload0 = pack_signed_pair(0, 0);
  changes[0].payload1 = pack_block(0, 7u);

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = 1u;
  snapshot.tick_id = 1u;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(changes));

  return octaryn_client_apply_server_snapshot(&snapshot);
}

bool load_bundled_game_module_descriptor() {
  char directory_buffer[4096] = {};
  if (!build_client_bundle_path(directory_buffer, sizeof(directory_buffer),
                                "Data/Module",
                                "game_module_descriptor_path=failed")) {
    return false;
  }

  const std::filesystem::path directory(directory_buffer);
  if (!std::filesystem::exists(directory)) {
    log_line("game_module_descriptor_path=failed");
    return false;
  }

  std::vector<std::filesystem::path> manifests;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() &&
        entry.path().filename().string().ends_with(".module.json")) {
      manifests.push_back(entry.path());
    }
  }

  if (manifests.empty()) {
    log_line("game_module_descriptor_path=failed");
    return false;
  }

  std::sort(manifests.begin(), manifests.end());
  std::string payload;
  if (!read_text_file(manifests.front().string().c_str(),
                      "game_module_descriptor=open_failed", payload)) {
    return false;
  }

  if (payload.find("\"ModuleId\"") == std::string::npos) {
    log_line("game_module_descriptor=invalid");
    return false;
  }

  log_line("game_module_descriptor=loaded");
  return true;
}

void log_chunk_view_if_changed(uint64_t frame_index,
                               const octaryn_client_chunk_view &view,
                               octaryn_client_chunk_view &logged_view) {
  if (octaryn_client_chunk_view_equal(&view, &logged_view) != 0 &&
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

} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  open_log();
  octaryn_client_function_profile_configure(g_log);
  octaryn_native_crash_diagnostics_init("octaryn-client-app");
  if (g_log != nullptr) {
    std::fprintf(g_log, "crash_marker=%s\n",
                 octaryn_native_crash_diagnostics_marker_path());
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    log_line("sdl_init=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      close_log();
    }
    return 2;
  }

  SDL_Window *window = SDL_CreateWindow(
      "Octaryn", kWindowWidth, kWindowHeight,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
  if (window == nullptr) {
    log_line("window_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_Quit();
    close_log();
    return 3;
  }

  if (!octaryn_client_window_lifecycle_show(window)) {
    log_line("window_show=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 4;
  }
  octaryn_client_window_lifecycle_finish_show(window);
  log_line("window_show=0");

  SDL_GPUDevice *gpu_device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
  if (gpu_device == nullptr) {
    log_line("gpu_device_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 7;
  }
  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
    log_line("gpu_window_claim=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 7;
  }
  log_line("gpu_device_create=0");
  log_line("gpu_window_claim=0");
  octaryn_client_frame_pacing frame_pacing{};
  octaryn_client_frame_pacing_init(&frame_pacing);
  octaryn_client_swapchain_state swapchain_state{};
  octaryn_client_swapchain_state_init(&swapchain_state);
  if (!octaryn_client_swapchain_configure(&swapchain_state, gpu_device, window,
                                          &frame_pacing)) {
    log_line("gpu_swapchain_configure=failed");
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 7;
  }
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "gpu_swapchain_configure=0 present_mode=%s fps_cap=%d\n",
                 octaryn_client_swapchain_present_mode_name(&swapchain_state),
                 frame_pacing.fps_cap);
    std::fflush(g_log);
  }

  const bool game_modules_disabled = read_enabled_flag(kDisableGameModulesFlag);
  if (g_log != nullptr) {
    std::fprintf(g_log, "client_game_module_load=%s\n",
                 game_modules_disabled ? "disabled" : "enabled");
    std::fflush(g_log);
  }
  singleplayer_server_session server_session{};
  prepare_singleplayer_server_session(server_session, game_modules_disabled);

  ClientBlockAtlas atlas{};
  if (game_modules_disabled) {
    log_line("game_module_descriptor=skipped reason=disabled");
    log_line("block_atlas=skipped reason=disabled");
    if (!load_native_empty_world_atlas(gpu_device, atlas)) {
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 10;
    }
  } else if (!load_bundled_game_module_descriptor() ||
             !load_client_block_atlas(gpu_device, g_log, atlas)) {
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 10;
  }

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;

  int result = octaryn_client_initialize(&api);
  log_result("initialize", result);
  if (result != 0) {
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 5;
  }

  if (read_enabled_flag("OCTARYN_CLIENT_APP_PRESENTATION_PROBE_SNAPSHOT")) {
    result = apply_probe_snapshot();
    log_result("presentation_probe_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_client_block_atlas(atlas);
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 8;
    }
  }

  std::vector<presentation_block> world_snapshot_blocks;
  std::vector<presentation_block> world_surface_blocks;
  server_world_time_state world_time{};
  if (server_session.enabled) {
    log_line("world_blocks_snapshot=deferred source=singleplayer_server");
  } else if (game_modules_disabled) {
    log_line("world_blocks_snapshot=skipped reason=game_modules_disabled");
    log_line("live_chunk_streaming active=1 source=client_native_empty_world");
  } else if (!load_world_snapshot_blocks(world_snapshot_blocks,
                                         world_surface_blocks, world_time)) {
    octaryn_client_shutdown();
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 9;
  }

  if (!world_snapshot_blocks.empty()) {
    result = apply_snapshot_blocks(world_snapshot_blocks, 2u);
    log_result("world_blocks_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_client_block_atlas(atlas);
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 10;
    }
  }
  block_lookup world_block_lookup = build_block_lookup(world_snapshot_blocks);
  if (g_log != nullptr) {
    std::fprintf(g_log, "live_block_interaction_lookup blocks=%zu\n",
                 world_block_lookup.size());
    std::fflush(g_log);
  }

  client_shader_pipelines shader_pipelines{};
  if (!initialize_shader_pipelines(gpu_device, window, shader_pipelines)) {
    octaryn_client_shutdown();
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 11;
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  octaryn_client_runtime_controls runtime_controls{};
  octaryn_client_runtime_controls_init(&runtime_controls);
  if (octaryn_client_runtime_settings_load(window, &runtime_controls) == 0) {
    log_line("client_settings_load=failed");
  } else {
    log_line("client_settings_load=0");
  }
  octaryn_client_runtime_controls_refresh_menu(&runtime_controls, window,
                                               kWindowWidth, kWindowHeight);
  octaryn_client_runtime_controls_sync_relative_mouse(&runtime_controls,
                                                      window);
  frame_pacing.requested_present_mode =
      runtime_controls.present_mode_index == 0
          ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_IMMEDIATE
          : (runtime_controls.present_mode_index == 1
                 ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_MAILBOX
                 : OCTARYN_CLIENT_PRESENT_MODE_POLICY_VSYNC);
  if (octaryn_client_swapchain_configure(&swapchain_state, gpu_device, window,
                                         &frame_pacing) &&
      g_log != nullptr) {
    std::fprintf(g_log,
                 "gpu_swapchain_configure=0 source=settings present_mode=%s "
                 "fps_cap=%d\n",
                 octaryn_client_swapchain_present_mode_name(&swapchain_state),
                 frame_pacing.fps_cap);
    std::fflush(g_log);
  }
  octaryn_client_frame_metrics frame_metrics{};
  octaryn_client_frame_metrics_init(&frame_metrics);
  octaryn_client_frame_profile_snapshot last_profile{};
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_runtime_controls active=1 f3=1 f11=1 escape_menu=1 "
                 "menu=%u debug=%u render_distance=%d\n",
                 static_cast<unsigned>(runtime_controls.display_menu.active),
                 static_cast<unsigned>(runtime_controls.debug_overlay_enabled),
                 runtime_controls.render_distance);
    std::fflush(g_log);
  }
  octaryn_client_fly_player_controller player{};
  octaryn_client_fly_player_controller_init(&player);
  place_camera_over_snapshot(player.camera, world_surface_blocks);
  octaryn_client_camera_update(&player.camera);
  block_selection_state block_selection{};
  if (!game_modules_disabled) {
    block_selection.selected_block = client_block_atlas_default_placeable_block(
        atlas, kDefaultInteractionPlaceBlock);
  } else {
    block_selection.selected_block = 1u;
  }
  if (g_log != nullptr && !game_modules_disabled) {
    const int32_t layer = client_block_atlas_top_layer_for_block(
        atlas, block_selection.selected_block);
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
  octaryn_client_chunk_view native_empty_mesh_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  client_key_state keys{};
  client_world_time_controls world_time_controls{};
  octaryn_client_chunk_view logged_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  client_server_stream_poll_state server_stream_poll{};
  server_stream_poll.active_server_stream_override_signature =
      std::numeric_limits<uint64_t>::max();
  if (server_session.enabled) {
    const octaryn_client_chunk_view initial_chunk_view =
        octaryn_client_chunk_view_for_camera(player.camera.position[0],
                                             player.camera.position[2],
                                             runtime_controls.render_distance);
    octaryn_client_chunk_view empty_previous_view{
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
    octaryn_client_frame_profile_sample profile_sample{};
    const uint64_t misc_start = frame_start_ticks;
    pointer_motion_debug_state pointer_motion{};
    pointer_click_debug_state pointer_click{};
    poll_client_app_events(window, gpu_device, frame_pacing, swapchain_state,
                           runtime_controls, keys, world_time_controls,
                           block_selection, atlas, game_modules_disabled,
                           pointer_motion, pointer_click, running,
                           frame_index + 1u);
    profile_sample.misc_ms +=
        octaryn_client_frame_profile_elapsed_ms_since(misc_start);

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
    if (octaryn_client_runtime_controls_ui_active(&runtime_controls) != 0u) {
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
    const octaryn_client_chunk_view chunk_view =
        octaryn_client_chunk_view_for_camera(camera.position[0],
                                             camera.position[2],
                                             runtime_controls.render_distance);
    log_chunk_view_if_changed(frame.timing.frame_index, chunk_view,
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
    const bool native_empty_local_edit =
        game_modules_disabled && selection_hit.has_hit &&
        ((input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u);
    if (!write_block_interaction_intent(
            frame, input, camera, selection_hit, block_selection.selected_block,
            world_snapshot_blocks, world_block_lookup, game_modules_disabled)) {
      result = -8;
      running = false;
      break;
    }
    bool native_empty_stream_mesh_dirty = false;
    if (!poll_server_stream_presentation(
            server_session, game_modules_disabled, native_empty_mesh_chunk_view,
            frame.timing.frame_index, server_stream_poll, world_time,
            world_snapshot_blocks, world_surface_blocks, world_block_lookup,
            player.camera, native_empty_stream_mesh_dirty, result)) {
      running = false;
      break;
    }
    previous_ticks = current_ticks;
    log_client_tick_input_frame(frame);
    profile_sample.sim_ms =
        octaryn_client_frame_profile_elapsed_ms_since(sim_start);

    const uint64_t world_start = SDL_GetTicksNS();
    result = octaryn_client_tick(&frame);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    if (game_modules_disabled) {
      if (server_session.enabled &&
          (native_empty_stream_mesh_dirty || native_empty_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        octaryn_client_function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "server_background");
        const server_chunk_stream_file &active_server_stream =
            server_stream_poll.active_server_stream;
        const octaryn_client_chunk_view mesh_chunk_view =
            !active_server_stream.columns.empty()
                ? chunk_view_from_server_stream(active_server_stream)
                : chunk_view;
        if (!active_server_stream.columns.empty()) {
          build_native_empty_world_mesh_frame_from_stream(
              active_server_stream, world_block_lookup,
              native_empty_mesh_chunk_view, mesh_upload_frame);
        } else {
          build_native_empty_world_mesh_frame(
              chunk_view, native_empty_mesh_chunk_view, world_block_lookup,
              mesh_upload_frame);
        }
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        native_empty_mesh_chunk_view = mesh_chunk_view;
        if (visible_world_mesh_frame.chunks.empty()) {
          release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
        } else {
          octaryn_client_function_profile_scope upload_profile_scope(
              "world_mesh_upload", frame.timing.frame_index,
              "native_empty_server");
          if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers,
                                       frame.timing.frame_index)) {
            result = -6;
            running = false;
            break;
          }
        }
      } else if (!server_session.enabled &&
                 (!same_chunk_view(native_empty_mesh_chunk_view, chunk_view) ||
                  native_empty_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        octaryn_client_function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "client_native");
        build_native_empty_world_mesh_frame(
            chunk_view, native_empty_mesh_chunk_view, world_block_lookup,
            mesh_upload_frame);
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        native_empty_mesh_chunk_view = chunk_view;
        if (visible_world_mesh_frame.chunks.empty()) {
          release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
        } else {
          octaryn_client_function_profile_scope upload_profile_scope(
              "world_mesh_upload", frame.timing.frame_index,
              "native_empty_client");
          if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers,
                                       frame.timing.frame_index)) {
            result = -6;
            running = false;
            break;
          }
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
        octaryn_client_function_profile_scope upload_profile_scope(
            "world_mesh_upload", frame.timing.frame_index, "game_module");
        if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                     mesh_buffers, frame.timing.frame_index)) {
          result = -6;
          running = false;
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
        octaryn_client_frame_profile_elapsed_ms_since(world_start);

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
    profile_sample.total_ms = octaryn_client_frame_profile_elapsed_ms(
        frame_start_ticks, frame_end_ticks);
    octaryn_client_frame_profile_finalize_sample(&profile_sample);
    octaryn_client_frame_metrics_record(&frame_metrics, profile_sample.total_ms,
                                        frame_end_ticks);
    last_profile.sample = profile_sample;
    last_profile.metrics = octaryn_client_frame_metrics_snapshot_value(
        &frame_metrics, frame_end_ticks);
    log_frame_profile(frame.timing.frame_index, last_profile,
                      runtime_controls.debug_overlay_enabled);

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }
  }

  stop_singleplayer_server(server_session);
  octaryn_client_shutdown();
  log_line("shutdown=0");
  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  release_shader_pipelines(gpu_device, shader_pipelines);
  destroy_client_block_atlas(atlas);
  SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
  SDL_DestroyGPUDevice(gpu_device);
  SDL_DestroyWindow(window);
  SDL_Quit();

  close_log();

  return result == 0 ? 0 : 6;
}
