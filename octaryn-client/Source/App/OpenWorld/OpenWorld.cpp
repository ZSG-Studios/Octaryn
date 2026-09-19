#include "OpenWorld.h"
#include "Controls.h"
#include "LocalSession.h"
#include "MainMenu.h"
#include "LoadingScreen.h"
#include "WorldSession.h"
#include "WorldProfile.h"
#include "WorldRenderer.h"
#include "BlockInteraction.h"
#include "RuntimeSettings.h"
#include "LightingPanel.h"
#include "GameUi.h"
#include "ActionAudio.h"
#include "ActionSounds.h"
#include "StreamingBenchmark.h"
#include "RendererStartup.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
namespace fs = std::filesystem;
namespace graphics = octaryn::client::rendering;

int run_window(SDL_Window* window, const WorldRunOptions& options) {
  auto bundle = bundle_path(SDL_GetBasePath());
  if (bundle.filename().empty()) bundle = bundle.parent_path();
  const auto root = repo_root(bundle);
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
      start_renderer(window, controls.running), graphics::open_world_renderer_destroy);
  auto* renderer = renderer_owner.get();
  if (!renderer) {
    if(!controls.running)return 0;
    std::fprintf(stderr, "Slang RHI world renderer initialization failed\n");
    return 1;
  }
  controls.ui.ray_tracing_available=graphics::open_world_renderer_stats(renderer).ray_tracing_available?1:0;
  pump_boot_stage(window, "renderer_ready");
  world_presentation::BlockInteraction interaction;
  if (!interaction.load_catalog(bundle / "Data" / "Blocks" / "octaryn.basegame.blocks.json"))
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
            const SessionOutcome outcome = run_world_session(session_ctx, session);
            session.stop();
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
  SDL_Window* window = SDL_CreateWindow(menu_boot_requested(options) ? "Octaryn | Main Menu" : "Octaryn | Loading world", 1280, 720,
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
