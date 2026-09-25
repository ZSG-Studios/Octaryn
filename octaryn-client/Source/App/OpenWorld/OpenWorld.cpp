#include "OpenWorld.h"
#include "AppPaths.h"
#include "Controls.h"
#include "MapManifest.h"
#include "MapPlayer.h"
#include "LightingState.h"
#include "MapWorldSession.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "FrameWatchdog.h"
#include "RuntimeSettings.h"
#include "ActionAudio.h"
#include "ActionSounds.h"
#include "DebugOverlay.h"
#include "RendererStartup.h"
#include "MapStartup.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
namespace fs = std::filesystem;
namespace graphics = octaryn::client::rendering;

template<typename Release>
void shutdown_stage(const char* stage,Release&& release) {
  const auto start=SDL_GetTicksNS();
  std::printf("client_shutdown stage=%s status=begin elapsed_ms=0.000\n",stage);
  std::fflush(stdout);
  release();
  std::printf("client_shutdown stage=%s status=end elapsed_ms=%.3f\n",stage,
      static_cast<double>(SDL_GetTicksNS()-start)/1e6);
  std::fflush(stdout);
}

void retire_renderer(graphics::WorldRenderer* renderer) {
  auto started=SDL_GetTicksNS();
  if(!graphics::open_world_renderer_begin_retirement(renderer))
    throw std::runtime_error("Renderer retirement synchronization failed");
  graphics::WorldRetirementGuard guard;
  const auto timeout=std::min<std::uint64_t>(graphics::frame_fence_timeout_ms(),UINT64_MAX/1000000ull)*1000000ull;
  guard.observe(graphics::open_world_renderer_retirement_progress(renderer),SDL_GetTicksNS(),timeout);
  std::uint64_t remaining{};
  std::uint32_t batch=8,quick_frames{};
  do {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {}
    const auto release_started=SDL_GetTicksNS();
    remaining=graphics::open_world_renderer_retire_step(renderer,batch);
    const auto released=SDL_GetTicksNS();
    if(!graphics::open_world_renderer_retirement_frame(renderer))
      throw std::runtime_error("Renderer closing frame failed");
    const auto progress=graphics::open_world_renderer_retirement_progress(renderer);
    remaining=progress.remaining();
    const auto completed=SDL_GetTicksNS();
    std::printf("client_shutdown_progress owners=%llu pending_count=%llu pending_buffer_bytes=%llu\n",
        static_cast<unsigned long long>(progress.owners),static_cast<unsigned long long>(progress.pending_count),
        static_cast<unsigned long long>(progress.pending_buffer_bytes));
    if(remaining && !guard.observe(progress,completed,timeout))
      graphics::frame_gpu_shutdown_failed("renderer_retirement_progress");
    const double work_ms=double(completed-release_started)/1e6;
    const auto used_batch=batch;
    // The GPU fence and deferred resource destruction are included. Sleep
    // overshoot never controls how many resources the next frame admits.
    if(work_ms>24.0) {batch=std::max(1u,batch/2);quick_frames=0;}
    else if(work_ms<12.0 && ++quick_frames>=2) {batch=std::min(32u,batch+2);quick_frames=0;}
    else if(work_ms>=12.0)quick_frames=0;
    const auto elapsed=completed-started;
    constexpr Uint64 interval=1000000000ull/30;
    if(elapsed<interval)SDL_DelayNS(interval-elapsed);
    std::printf("client_shutdown_frame total_ms=%.6f remaining=%llu\n",
        static_cast<double>(SDL_GetTicksNS()-started)/1e6,static_cast<unsigned long long>(remaining));
    std::printf("client_shutdown_work release_ms=%.6f maintenance_ms=%.6f batch=%u next_batch=%u\n",
        double(released-release_started)/1e6,double(completed-released)/1e6,used_batch,batch);
    std::fflush(stdout);
    started=SDL_GetTicksNS();
  } while(remaining);
}

int run_window(SDL_Window* window, const WorldRunOptions& options) {
  auto bundle = bundle_path(SDL_GetBasePath());
  if (bundle.filename().empty()) bundle = bundle.parent_path();
  const auto root = repo_root(bundle);
  fs::create_directories(root / "logs" / "client");
  WorldProfile profile(root / "logs" / "client" / "open-world.csv");
  WorldControls controls;
  controls.third_person=options.third_person;
  controls.shoulder=options.shoulder;
  runtime_controls_init(&controls.ui);
  controls.ui.debug_overlay_enabled=options.show_diagnostics?1:0;
  if (options.benchmark_seconds <= 0 || options.benchmark_settings) {
    const bool loaded=runtime_settings_load(window, &controls.ui)!=0;
    if (options.benchmark_hidden && !loaded)throw std::runtime_error("Hidden capture settings could not be applied");
  }
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  pump_boot_stage(window, "window_ready");
  std::unique_ptr<graphics::WorldRenderer, decltype(&graphics::open_world_renderer_destroy)> renderer_owner(
      start_renderer(window, controls.running, controls.ui), graphics::open_world_renderer_destroy);
  auto* renderer = renderer_owner.get();
  if (!renderer) {
    if(!controls.running)return 0;
    std::fprintf(stderr, "Slang RHI world renderer initialization failed\n");
    return 1;
  }
  controls.ui.ray_tracing_available=graphics::open_world_renderer_stats(renderer).ray_tracing_available?1:0;
  pump_boot_stage(window, "renderer_ready");
  if (!map_mode_available(bundle)) {
    std::fprintf(stderr, "No map manifest: the engine renders mesh map worlds; ship Assets/Maps/map.json\n");
    return 1;
  }
  MapManifest map_manifest;
  if (!load_map_manifest(bundle, map_manifest)) return 1;
  const auto glb_utf8 = map_manifest.glb.generic_u8string();
  if (!start_map(window, renderer, reinterpret_cast<const char*>(glb_utf8.c_str()), controls.running)) {
    if(!controls.running)return 0;
    std::fprintf(stderr, "Map load failed: %s\n", graphics::open_world_renderer_status(renderer));
    return 1;
  }
  bool sounds_present{};
  const auto sounds=load_action_sounds(bundle / "Assets" / "Audio" / "action-sounds.json", sounds_present);
  audio::ActionAudioOwner audio_owner;
  if (sounds_present) audio_owner.reset(audio::create_action_audio(sounds));
  if (sounds_present) {
    const auto audio_status=audio::action_audio_status(audio_owner.get());
    std::printf("action_audio available=%u status=%s\n",audio_status.available?1u:0u,
        audio_status.message?audio_status.message:"unknown");
    std::fflush(stdout);
  }
  LightingState lighting(window);
  auto overlay=std::make_unique<DebugOverlay>(window,graphics::open_world_renderer_ui_interface(renderer),
      bundle / "Assets" / "Ui",controls.ui);
  controls.overlay=overlay.get();
  graphics::open_world_renderer_set_ui_context(renderer,overlay->context());
  graphics::open_world_renderer_set_capture_enabled(renderer,!options.validate_frame_pacing);
  MapPlayer player;
  map_player_spawn(player, map_manifest.spawn_x, map_manifest.spawn_y, map_manifest.spawn_z,
      map_manifest.yaw, map_manifest.pitch);
  MapSessionContext session_ctx;
  session_ctx.window = window;
  session_ctx.options = &options;
  session_ctx.profile = &profile;
  session_ctx.controls = &controls;
  session_ctx.player = &player;
  session_ctx.overlay = overlay.get();
  session_ctx.lighting = &lighting;
  session_ctx.manifest = &map_manifest;
  session_ctx.width = &width;
  session_ctx.height = &height;
  session_ctx.renderer = renderer;
  int result = run_map_world_session(session_ctx).code;
  graphics::open_world_renderer_set_ui_context(renderer,nullptr);
  controls.overlay=nullptr;
  shutdown_stage("ui",[&] {overlay.reset();});
  shutdown_stage("renderer",[&] {retire_renderer(renderer);renderer_owner.reset();});
  runtime_settings_save(window, &controls.ui);
  lighting.save();
  return result;
}
}

int run_open_world(const WorldRunOptions& options) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("ZSG Engine | Loading map", 1280, 720,
      SDL_WINDOW_RESIZABLE | (options.benchmark_hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window) {
    std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  if(!SDL_SetWindowMinimumSize(window,options.benchmark_hidden?1:640,options.benchmark_hidden?1:480)) {
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
