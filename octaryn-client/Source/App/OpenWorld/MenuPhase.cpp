#include "MenuPhase.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "WorldRenderer.h"
#include "MapMode.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace octaryn::client::app {
namespace {

namespace graphics = octaryn::client::rendering;

std::string utf8(const std::filesystem::path& path) {
  const auto value = path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

bool cli_driven(const WorldRunOptions& options) {
  return options.frame_limit > 0 || options.benchmark_seconds > 0 ||
         options.validate_frame_pacing || !options.capture_ui.empty();
}

} // namespace

std::vector<MenuWorldEntry> scan_map_worlds(const std::filesystem::path& maps_root) {
  std::vector<MenuWorldEntry> worlds;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(maps_root, error)) {
    if (!entry.is_directory()) continue;
    const auto manifest = entry.path() / "map.json";
    if (!std::filesystem::is_regular_file(manifest)) continue;
    worlds.push_back({entry.path().filename().generic_string(), {}, manifest});
  }
  if (std::filesystem::is_regular_file(maps_root / "map.json"))
    worlds.push_back({"Main", {}, maps_root / "map.json"});
  return worlds;
}

MenuSelection run_menu_phase(SDL_Window* window, graphics::WorldRenderer* renderer,
                             WorldControls& controls, MenuStack& menu,
                             const std::filesystem::path& maps_root,
                             const WorldRunOptions& options) {
  MenuSelection selection;
  if (!options.connect_endpoint.empty()) {
    selection.kind = MenuSelection::Kind::Connect;
    selection.endpoint = options.connect_endpoint;
    return selection;
  }

  auto worlds = scan_map_worlds(maps_root);
  for (auto& world : worlds) {
    // Resolve the GLB through the manifest; entries without payloads are
    // dropped from the list.
    MapManifest manifest;
    if (!load_map_manifest_from(world.manifest, manifest)) continue;
    world.glb = manifest.glb;
  }
  worlds.erase(std::remove_if(worlds.begin(), worlds.end(),
                              [](const MenuWorldEntry& world) { return world.glb.empty(); }),
               worlds.end());
  menu.set_worlds(worlds);

  if (cli_driven(options)) {
    selection.kind = MenuSelection::Kind::World;
    if (!worlds.empty()) selection.world = worlds.front();
    return selection;
  }

  volatile bool chosen = false;
  MenuActions actions;
  actions.quit = [&] { selection.kind = MenuSelection::Kind::Quit; chosen = true; };
  actions.play_world = [&](const MenuWorldEntry& world) {
    selection.kind = MenuSelection::Kind::World;
    selection.world = world;
    chosen = true;
  };
  actions.connect_server = [&](const std::string& endpoint) {
    if (endpoint.empty()) { menu.set_connect_status("Enter host:port first."); return; }
    selection.kind = MenuSelection::Kind::Connect;
    selection.endpoint = endpoint;
    chosen = true;
  };
  menu.install_actions(actions);
  menu.show_main();

  const auto window_id = SDL_GetWindowID(window);
  while (controls.running && !chosen) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      menu.process_event(event);
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window_id))
        controls.running = false;
    }
    int width{}, height{};
    SDL_GetWindowSizeInPixels(window, &width, &height);
    if (width > 0 && height > 0) {
      menu.update(width, height);
      graphics::open_world_renderer_render_menu_context(renderer, menu.context());
    }
    SDL_DelayNS(8'000'000);
  }
  menu.hide_pause();
  if (!controls.running) selection.kind = MenuSelection::Kind::Quit;
  return selection;
}

}
