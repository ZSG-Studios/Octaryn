#include "OpenWorld.h"
#include "Controls.h"
#include "LocalSession.h"
#include "MainMenu.h"
#include "LoadingScreen.h"
#include "MapMode.h"
#include "WorldSession.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "FrameWatchdog.h"
#include "BlockInteraction.h"
#include "RuntimeSettings.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "ActionAudio.h"
#include "ActionSounds.h"
#include "StreamingBenchmark.h"
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
  StreamingBenchmark streaming_benchmark(options.benchmark_streaming_speed,options.benchmark_route_origin);
  LightingPanel lighting(window);
  lighting.visible=options.show_lighting;
  WorldControls controls;
  controls.lighting=&lighting;
  controls.third_person=options.third_person;
  controls.shoulder=options.shoulder;
  runtime_controls_init(&controls.ui);
  controls.ui.render_distance=4;
  controls.ui.debug_overlay_enabled=options.show_diagnostics?1:0;
  if (options.benchmark_seconds <= 0 || options.benchmark_settings) {
    const bool loaded=runtime_settings_load(window, &controls.ui)!=0;
    if(options.benchmark_hidden && !loaded)throw std::runtime_error("Hidden capture settings could not be applied");
  }
  if (options.render_distance>0) controls.ui.render_distance=options.render_distance;
  runtime_controls_set_max_render_distance(&controls.ui, 32);
  const bool menu_boot = menu_boot_requested(options);
  controls.ui.session_active = menu_boot ? 0 : 1;
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  runtime_controls_refresh_menu(&controls.ui, window, width, height);
  if (menu_boot) display_menu_open_main(&controls.ui.display_menu);
  else if (options.show_settings) display_menu_open(&controls.ui.display_menu);
  unsigned radius = static_cast<unsigned>(controls.ui.render_distance);
  const bool remote = !options.connect_endpoint.empty();
  const char* world_override = SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  fs::path world = remote && !(world_override && *world_override)
      ? root / "remote-cache" : default_world_path(root);
  pump_boot_stage(window, "window_ready");
  LocalSession session;
  const bool qualification=options.validate_world_items || options.validate_block_actions || options.validate_temporal ||
      options.validate_lighting_motion || options.validate_lighting_edits;
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
  const bool map_mode = map_mode_available(bundle);
  MapManifest map_manifest;
  if (map_mode) {
    if (!load_map_manifest(bundle, map_manifest)) return 1;
    const auto glb_utf8 = map_manifest.glb.generic_u8string();
    if (!start_map(window, renderer, reinterpret_cast<const char*>(glb_utf8.c_str()), controls.running)) {
      if(!controls.running)return 0;
      std::fprintf(stderr, "Map load failed: %s\n", graphics::open_world_renderer_status(renderer));
      return 1;
    }
  }
  world_presentation::BlockInteraction interaction;
  if (!map_mode && !interaction.load_catalog(bundle / "Data" / "Blocks" / "octaryn.basegame.blocks.json"))
    throw std::runtime_error("Cannot load the basegame block interaction catalog");
  audio::ActionAudioOwner audio_owner(audio::create_action_audio(
      load_action_sounds(bundle / "Assets" / "Audio" / "action-sounds.json")));
  const auto audio_status=audio::action_audio_status(audio_owner.get());
  std::printf("action_audio available=%u status=%s\n",audio_status.available?1u:0u,
      audio_status.message?audio_status.message:"unknown");
  std::fflush(stdout);
  fs::path palette;
  if (!menu_boot) {
    const char* palette_override=SDL_getenv("OCTARYN_CLIENT_INVENTORY_PATH");
    palette=palette_override && *palette_override?bundle_path(palette_override):world/"client"/"inventory.json";
    if(!palette_override) seed_inventory_palette(palette, root/"settings"/"build-palette.json");
  }
  auto game_ui=std::make_unique<GameUi>(window,graphics::open_world_renderer_ui_interface(renderer),
      bundle / "Assets" / "Ui",controls.ui,lighting,palette);
  controls.game_ui=game_ui.get();
  graphics::open_world_renderer_set_ui_context(renderer,game_ui->context());
  graphics::open_world_renderer_set_capture_enabled(renderer,!qualification || options.validate_lighting_edits);
  int result = 0;
  bool show_loading = false;
  bool autoplay_used = false;
  bool in_menu = menu_boot;
  if (!menu_boot && remote && !session.start_remote(bundle, world, radius, options.connect_endpoint, root / "logs" / "server")) {
    std::fprintf(stderr, "Remote server startup failed: %s\n", session.status().c_str());
    return 1;
  }
  if (!menu_boot && !remote && !session.start(bundle, world, radius, qualification?world/"logs"/"server":root/"logs"/"server")) {
    std::fprintf(stderr, "Local server startup failed: %s\n", session.status().c_str());
    return 1;
  }
    unsigned qualified_sessions{};
    while (controls.running) {
    if (in_menu) {
      MenuPhase phase;
      phase.window = window;
      phase.options = &options;
      phase.root = root;
      phase.bundle = bundle;
      phase.profile = &profile;
      phase.controls = &controls;
      phase.radius = &radius;
      phase.world = &world;
      phase.width = &width;
      phase.height = &height;
      phase.renderer = renderer;
      phase.ui = game_ui.get();
      phase.session = &session;
            phase.autoplay_consumed = autoplay_used;
            phase.qualification_rejoin = options.validate_session_rejoin && qualified_sessions > 0;
      if (run_menu_phase(phase) == MenuEnd::Quit) break;
      autoplay_used = true;
      show_loading = true;
      in_menu = false;
    } else {
      WorldSession session_ctx;
      session_ctx.window = window;
      session_ctx.options = &options;
      session_ctx.profile = &profile;
      session_ctx.streaming = &streaming_benchmark;
      session_ctx.lighting = &lighting;
      session_ctx.controls = &controls;
      session_ctx.radius = &radius;
      session_ctx.world = &world;
      session_ctx.width = &width;
      session_ctx.height = &height;
      session_ctx.renderer = renderer;
      session_ctx.interaction = &interaction;
      session_ctx.audio = &audio_owner;
      session_ctx.ui = game_ui.get();
      session_ctx.show_loading = show_loading;
      session_ctx.remote_authority = remote;
            const SessionOutcome outcome = map_mode
                ? run_map_world_session(session_ctx, session)
                : run_world_session(session_ctx, session);
            shutdown_stage("server",[&] {session.stop();});
            if (options.validate_session_rejoin && outcome.disconnect && outcome.code == 0) {
                ++qualified_sessions;
                std::printf("session_rejoin completed=%u target=3\n", qualified_sessions);
                if (qualified_sessions == 3) {
                    std::puts("session_rejoin=passed sessions=3 menu_returns=2");
                    result = 0;
                    break;
                }
            }
      if (outcome.disconnect && outcome.code == 0) {
                // Evict the old world while preserving a valid loading-frame
                // draw distance until the next authoritative pose sets the center.
                graphics::open_world_renderer_set_center(renderer, 4000000, 4000000, static_cast<int>(radius));
        in_menu = true;
      } else {
        result = outcome.code;
        break;
      }
    }
  }
  graphics::open_world_renderer_set_ui_context(renderer,nullptr);
  controls.game_ui=nullptr;
  shutdown_stage("ui",[&] {game_ui.reset();});
  shutdown_stage("renderer",[&] {retire_renderer(renderer);renderer_owner.reset();});
  session.stop();
  return result;
}
}

int run_open_world(const WorldRunOptions& options) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow(menu_boot_requested(options) ? "Octaryn | Main Menu" : "Octaryn | Loading world", 1280, 720,
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
