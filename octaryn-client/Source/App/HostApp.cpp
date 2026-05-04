#include "BlockAtlas.h"
#include "EmptyWorldAtlas.h"
#include "Environment.h"
#include "FileIO.h"
#include "FrameLoop.h"
#include "HostCommands.h"
#include "Log.h"
#include "PresentationState.h"
#include "ShaderPipelines.h"
#include "WorldStream.h"
#include "FunctionProfile.h"
#include "HostExports.h"
#include "octaryn_client_swapchain.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_native_crash_diagnostics.h"
#include "SingleplayerServerSession.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using octaryn::client::rendering::BlockAtlas;
using octaryn::client::rendering::load_block_atlas;
using octaryn_client_app::apply_snapshot_blocks;
using octaryn_client_app::block_lookup;
using octaryn_client_app::build_client_bundle_path;
using octaryn_client_app::client_shader_pipelines;
using octaryn_client_app::close_log;
using octaryn_client_app::enqueue_command;
using octaryn_client_app::g_log;
using octaryn_client_app::initialize_shader_pipelines;
using octaryn_client_app::load_empty_world_atlas;
using octaryn_client_app::load_world_snapshot_blocks;
using octaryn_client_app::log_line;
using octaryn_client_app::log_result;
using octaryn_client_app::open_log;
using octaryn_client_app::pack_block;
using octaryn_client_app::pack_signed_pair;
using octaryn_client_app::prepare_singleplayer_server_session;
using octaryn_client_app::presentation_block;
using octaryn_client_app::read_enabled_flag;
using octaryn_client_app::read_text_file;
using octaryn_client_app::release_shader_pipelines;
using octaryn_client_app::run_frame_loop;
using octaryn_client_app::server_world_time_state;
using octaryn_client_app::singleplayer_server_session;
using octaryn_client_app::stop_singleplayer_server;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr const char *kDisableGameModulesFlag =
    "OCTARYN_CLIENT_DISABLE_GAME_MODULES";

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

} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  open_log();
  octaryn_native_crash_diagnostics_init("octaryn-client-app");
  if (g_log != nullptr) {
    std::fprintf(g_log, "crash_marker=%s\n",
                 octaryn_native_crash_diagnostics_marker_path());
  }
  function_profile_configure(g_log);

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

  BlockAtlas atlas{};
  if (game_modules_disabled) {
    log_line("game_module_descriptor=skipped reason=disabled");
    log_line("block_atlas=skipped reason=disabled");
    if (!load_empty_world_atlas(gpu_device, atlas)) {
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 10;
    }
  } else if (!load_bundled_game_module_descriptor() ||
             !load_block_atlas(gpu_device, g_log, atlas)) {
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
    destroy_block_atlas(atlas);
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
      destroy_block_atlas(atlas);
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
    destroy_block_atlas(atlas);
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
      destroy_block_atlas(atlas);
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
    destroy_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 11;
  }

  result = run_frame_loop(gpu_device, window, atlas, game_modules_disabled,
                          server_session, frame_pacing, swapchain_state,
                          shader_pipelines, world_snapshot_blocks,
                          world_surface_blocks, world_time, world_block_lookup);

  stop_singleplayer_server(server_session);
  octaryn_client_shutdown();
  log_line("shutdown=0");
  release_shader_pipelines(gpu_device, shader_pipelines);
  destroy_block_atlas(atlas);
  SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
  SDL_DestroyGPUDevice(gpu_device);
  SDL_DestroyWindow(window);
  SDL_Quit();

  close_log();

  return result == 0 ? 0 : 6;
}
