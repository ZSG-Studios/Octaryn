#pragma once
#include <filesystem>
#include <string>

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
std::filesystem::path default_world_path(const std::filesystem::path& root,const std::filesystem::path& bundle);
// Copies the prior build palette into a fresh world when present, creating
// the destination directory. A missing source is fine: the inventory falls
// back to defaults and saves on change.
void seed_inventory_palette(
    const std::filesystem::path& palette,
    const std::filesystem::path& prior);
// Validates the dedicated-server connection form.
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
  std::string* active_endpoint{};
  bool autoplay_consumed{};
  bool qualification_rejoin{};
  std::string feedback;
  std::string loading_detail;
  bool feedback_failed{true};
};
enum class MenuEnd { Quit, WorldReady, Failed };
// Runs menu frames until quit or a world is chosen and its session is
// started. On WorldReady the palette is retargeted and the loading overlay
// is shown; the caller proceeds straight into the world session.
MenuEnd run_menu_phase(MenuPhase& phase);
}
