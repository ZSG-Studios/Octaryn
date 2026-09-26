#pragma once
#include "MenuStack.h"
#include <filesystem>
#include <string>
#include <vector>

struct SDL_Window;
struct WorldRunOptions;
struct WorldControls;
namespace octaryn::client::rendering { struct WorldRenderer; }

namespace octaryn::client::app {

struct MenuSelection {
    enum class Kind { Quit, World, Connect } kind{Kind::Quit};
    MenuWorldEntry world;
    std::string endpoint;
};

// Scans Assets/Maps for playable worlds: each subdirectory with a map.json,
// plus a flat top-level map.json.
std::vector<MenuWorldEntry> scan_map_worlds(const std::filesystem::path& maps_root);

// Runs the boot menu. CLI-driven runs (frames/benchmark/connect) bypass the
// interactive menu and select automatically.
MenuSelection run_menu_phase(SDL_Window* window, rendering::WorldRenderer* renderer,
                             WorldControls& controls, MenuStack& menu,
                             const std::filesystem::path& maps_root,
                             const WorldRunOptions& options);

}
