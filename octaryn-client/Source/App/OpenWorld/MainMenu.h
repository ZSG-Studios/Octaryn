#pragma once
#include <filesystem>

struct SDL_Window;
struct runtime_controls;

namespace octaryn::client::rendering { struct WorldRenderer; }

namespace octaryn::client::app {
struct WorldRunOptions;
struct WorldControls;
class GameUi;
class LocalSession;
class WorldProfile;

// True for interactive desktop launches: boot to the main menu and defer the
// authoritative session until the player picks a world or server. Automated
// runs (benchmarks, validations, frame captures, debug views) keep the direct
// world startup so qualification stays deterministic.
bool menu_boot_requested(const WorldRunOptions& options);
// Checkout-relative roots shared by the menu and the world session.
std::filesystem::path bundle_path(const char* base);
std::filesystem::path repo_root(const std::filesystem::path& bundle);
// Menu world slots. Slot zero preserves the historical open-world-v3 default;
// the other slots are fresh singleplayer worlds. An explicit
// OCTARYN_CLIENT_WORLD_PATH override replaces slot zero.
std::filesystem::path default_world_path(const std::filesystem::path& root);
std::filesystem::path world_slot_path(const std::filesystem::path& root, unsigned slot);
void refresh_world_slots(const std::filesystem::path& root, runtime_controls* controls);
// Copies the prior build palette into a fresh world when present, creating
// the destination directory. A missing source is fine: the inventory falls
// back to defaults and saves on change.
void seed_inventory_palette(
    const std::filesystem::path& palette,
    const std::filesystem::path& prior);
// Handles singleplayer/multiplayer menu actions requested through the display
// menu. World creation/deletion touches only that slot directory. A validated
// remote address starts a remote session against a dedicated server and fills
// out_remote_endpoint; local worlds leave it empty. Returns true with out_world
// when a session should start.
bool consume_menu_world_request(
    const std::filesystem::path& root,
    runtime_controls& controls,
    std::filesystem::path& out_world,
    std::string& out_remote_endpoint);
// Persistent state shared with one menu phase. Scalars cross by reference so
// the menu can start a session and hand ownership back intact.
struct MenuPhase {
  SDL_Window* window{};
  const WorldRunOptions* options{};
  std::filesystem::path root, bundle;
  WorldProfile* profile{};
  WorldControls* controls{};
  unsigned* radius{};
  std::filesystem::path* world{};
  int* width{};
  int* height{};
  rendering::WorldRenderer* renderer{};
  GameUi* ui{};
  LocalSession* session{};
    bool autoplay_consumed{};
    bool qualification_rejoin{};
};
enum class MenuEnd { Quit, WorldReady };
// Runs menu frames until quit or a world is chosen and its session is
// started. On WorldReady the palette is retargeted and the loading overlay
// is shown; the caller proceeds straight into the world session.
MenuEnd run_menu_phase(MenuPhase& phase);
}
