#include "MainMenu.h"
#include "OpenWorld.h"
#include "RuntimeControls.h"
#include "Controls.h"
#include "GameUi.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include "WorldProfile.h"
#include "UiData.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace octaryn::client::app {
namespace graphics = octaryn::client::rendering;
namespace fs = std::filesystem;
namespace {
constexpr unsigned kWorldSlots = 3;

bool has_flag(bool value) { return value; }

std::string slot_dir(const std::filesystem::path& root, unsigned slot)
{
  if (slot == 0) return default_world_path(root).generic_string();
  return (root / "saves" / ("world-" + std::to_string(slot + 1))).generic_string();
}

bool slot_exists(const std::filesystem::path& root, unsigned slot)
{
  std::error_code error;
  return std::filesystem::is_directory(slot_dir(root, slot), error);
}

bool valid_port(const char* text)
{
  if (!text || !*text) return false;
  long value = 0;
  for (const char* c = text; *c; ++c)
  {
    if (*c < '0' || *c > '9') return false;
    value = value * 10 + (*c - '0');
    if (value > 65535) return false;
  }
  return value >= 1;
}

bool valid_address(const char* text)
{
  if (!text || !*text) return false;
  for (const char* c = text; *c; ++c)
  {
    const bool ok = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
        (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' || *c == '_';
    if (!ok) return false;
  }
  return true;
}

void write_world_name(const std::filesystem::path& slot_path, const char* name)
{
  if (!name || !*name || std::string_view(name) == "NEW WORLD") return;
  std::error_code error;
  std::filesystem::create_directories(slot_path, error);
  if (error) return;
  const auto marker = slot_path / "world_name.txt";
  FILE* file = nullptr;
#if defined(_WIN32)
  _wfopen_s(&file, marker.c_str(), L"w");
#else
  file = std::fopen(marker.c_str(), "w");
#endif
  if (file)
  {
    std::fputs(name, file);
    std::fclose(file);
  }
}
} // namespace

bool menu_boot_requested(const WorldRunOptions& options)
{
  if (!options.connect_endpoint.empty()) return false;  if (has_flag(options.benchmark_settings) || has_flag(options.benchmark_hidden)) return false;
  if (options.benchmark_seconds > 0 || options.frame_limit > 0) return false;
  if (!options.capture_ui.empty()) return false;
  if (has_flag(options.show_settings) || has_flag(options.show_menu) ||
      has_flag(options.show_inventory) || has_flag(options.show_creative) ||
      has_flag(options.show_fsr_settings))
    return false;
  if (has_flag(options.validate_ui) || has_flag(options.validate_distance_changes) ||
      has_flag(options.validate_world_items) || has_flag(options.validate_temporal) ||
      has_flag(options.validate_lighting_motion) || has_flag(options.validate_lighting_edits))
    return false;
  return true;
}

std::filesystem::path bundle_path(const char* base)
{
  if (!base) throw std::runtime_error("Missing application path");
  return std::filesystem::path(reinterpret_cast<const char8_t*>(base));
}

std::filesystem::path repo_root(const std::filesystem::path& bundle)
{
  auto path = bundle;
  if (path.filename().empty()) path = path.parent_path();
  const auto root = path.parent_path().parent_path().parent_path().parent_path();
  if (std::filesystem::exists(root / "CMakeLists.txt")) return root;
  char* pref = SDL_GetPrefPath("ZSGStudios", "Octaryn");
  if (!pref) throw std::runtime_error(SDL_GetError());
  std::filesystem::path fallback = bundle_path(pref);
  SDL_free(pref);
  return fallback;
}

std::filesystem::path default_world_path(const std::filesystem::path& root)
{
  const char* override_path = SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  if (override_path && *override_path) return bundle_path(override_path);
  return root / "saves" / "open-world-v3";
}

std::filesystem::path world_slot_path(const std::filesystem::path& root, unsigned slot)
{
  return std::filesystem::path(slot_dir(root, slot));
}

void refresh_world_slots(const std::filesystem::path& root, runtime_controls* controls)
{
  if (!controls) return;
  uint32_t mask = 0;
  for (unsigned slot = 0; slot < kWorldSlots; ++slot)
  {
    if (slot_exists(root, slot)) mask |= 1u << slot;
  }
  controls->display_menu.world_exists_mask = mask;
}

void seed_inventory_palette(
    const std::filesystem::path& palette,
    const std::filesystem::path& prior)
{
  std::error_code error;
  if (std::filesystem::exists(palette, error) || !std::filesystem::exists(prior, error)) return;
  std::filesystem::create_directories(palette.parent_path(), error);
  if (error) return;
  std::filesystem::copy_file(prior, palette, error);
}

bool consume_menu_world_request(
    const std::filesystem::path& root,
    runtime_controls& controls,
    std::filesystem::path& out_world,
    std::string& out_remote_endpoint)
{
  auto& menu = controls.display_menu;
  const uint32_t action = menu.action_requested;
  if (action == DISPLAY_MENU_ACTION_NONE) return false;
  menu.action_requested = DISPLAY_MENU_ACTION_NONE;
  out_remote_endpoint.clear();

  const unsigned slot = menu.world_slot < kWorldSlots ? menu.world_slot : 0;
  const auto slot_path = world_slot_path(root, slot);
  switch (action)
  {
    case DISPLAY_MENU_ACTION_LOAD_WORLD:
      if (!slot_exists(root, slot))
      {
        menu.status_code = DISPLAY_MENU_STATUS_MISSING_WORLD;
        return false;
      }
      write_world_name(slot_path, menu.world_name);
      menu.status_code = DISPLAY_MENU_STATUS_LOADED;
      out_world = slot_path;
      return true;
    case DISPLAY_MENU_ACTION_CREATE_WORLD: {
      unsigned free_slot = kWorldSlots;
      for (unsigned index = 0; index < kWorldSlots; ++index)
      {
        if (!slot_exists(root, index))
        {
          free_slot = index;
          break;
        }
      }
      if (free_slot >= kWorldSlots)
      {
        menu.status_code = DISPLAY_MENU_STATUS_WORLD_EXISTS;
        return false;
      }
      const auto fresh = world_slot_path(root, free_slot);
      std::error_code error;
      std::filesystem::create_directories(fresh / "client", error);
      if (error)
      {
        menu.status_code = DISPLAY_MENU_STATUS_FAILED;
        return false;
      }
      write_world_name(fresh, menu.world_name);
      menu.world_slot = free_slot;
      menu.status_code = DISPLAY_MENU_STATUS_CREATED;
      refresh_world_slots(root, &controls);
      out_world = fresh;
      return true;
    }
    case DISPLAY_MENU_ACTION_DELETE_WORLD: {
      if (!slot_exists(root, slot))
      {
        menu.status_code = DISPLAY_MENU_STATUS_MISSING_WORLD;
        return false;
      }
      std::error_code error;
      std::filesystem::remove_all(slot_path, error);
      if (error)
      {
        menu.status_code = DISPLAY_MENU_STATUS_FAILED;
        return false;
      }
      menu.status_code = DISPLAY_MENU_STATUS_DELETED;
      refresh_world_slots(root, &controls);
      return false;
    }
    case DISPLAY_MENU_ACTION_SAVE_WORLD:
      menu.status_code = slot_exists(root, slot)
          ? DISPLAY_MENU_STATUS_SAVED
          : DISPLAY_MENU_STATUS_MISSING_WORLD;
      return false;
    case DISPLAY_MENU_ACTION_CONNECT_LOCAL: {
      const auto local = default_world_path(root);
      std::error_code error;
      std::filesystem::create_directories(local / "client", error);
      if (error)
      {
        menu.status_code = DISPLAY_MENU_STATUS_FAILED;
        return false;
      }
      menu.status_code = DISPLAY_MENU_STATUS_CONNECTED;
      refresh_world_slots(root, &controls);
      out_world = local;
      return true;
    }
    case DISPLAY_MENU_ACTION_CONNECT_SERVER: {
      if (!valid_address(menu.server_address) || !valid_port(menu.server_port))
      {
        menu.status_code = DISPLAY_MENU_STATUS_INVALID_SERVER;
        return false;
      }
      const auto remote_cache = root / "remote-cache";
      std::error_code error;
      std::filesystem::create_directories(remote_cache / "client", error);
      if (error)
      {
        menu.status_code = DISPLAY_MENU_STATUS_FAILED;
        return false;
      }
      menu.status_code = DISPLAY_MENU_STATUS_CONNECTED;
      refresh_world_slots(root, &controls);
      out_world = remote_cache;
      out_remote_endpoint = std::string(menu.server_address) + ":" + menu.server_port;
      return true;
    }
    default:
      break;
  }
  return false;
}

MenuEnd run_menu_phase(MenuPhase& phase)
{
  SDL_Window* window = phase.window;
  const WorldRunOptions& options = *phase.options;
  const std::filesystem::path& root = phase.root;
  const std::filesystem::path& bundle = phase.bundle;
  WorldProfile& profile = *phase.profile;
  WorldControls& controls = *phase.controls;
  unsigned& radius = *phase.radius;
  std::filesystem::path& world = *phase.world;
  int& width = *phase.width;
  int& height = *phase.height;
  auto* renderer = phase.renderer;
  GameUi* game_ui = phase.ui;
  LocalSession& session = *phase.session;
  controls.ui.session_active = 0;
  SDL_SetWindowTitle(window, "Octaryn | Main Menu");
  game_ui->show_main_menu();
  std::puts("client_main_menu ready=1 waiting_for_world=1");
  bool world_chosen = false;
  fs::path requested;
  std::string remote_endpoint;
  while (controls.running && !world_chosen) {
    refresh_world_slots(root, &controls.ui);
    read_world_controls(window, controls, true);
    if (!controls.running) break;
    if (options.play_world_slot > 0 && !phase.autoplay_consumed) {
      phase.autoplay_consumed = true;
      controls.ui.display_menu.world_slot = options.play_world_slot - 1;
      controls.ui.display_menu.action_requested = DISPLAY_MENU_ACTION_LOAD_WORLD;
    }
        bool requested_world = consume_menu_world_request(root, controls.ui, requested, remote_endpoint);
        if (phase.qualification_rejoin) {
            phase.qualification_rejoin = false;
            requested = world;
            remote_endpoint = options.connect_endpoint;
            requested_world = true;
        }
        if (requested_world) {
      const char* palette_override = SDL_getenv("OCTARYN_CLIENT_INVENTORY_PATH");
      const auto next_palette = palette_override && *palette_override
          ? bundle_path(palette_override) : requested / "client" / "inventory.json";
      if (!palette_override) seed_inventory_palette(next_palette, root / "settings" / "build-palette.json");
      if (!game_ui->retarget_palette(next_palette)) {
        controls.ui.display_menu.status_code = DISPLAY_MENU_STATUS_FAILED;
        game_ui->show_main_menu();
      } else {
        radius = static_cast<unsigned>(controls.ui.render_distance);
        const bool started = remote_endpoint.empty()
            ? session.start(bundle, requested, radius, root / "logs" / "server")
            : session.start_remote(bundle, requested, radius, remote_endpoint, root / "logs" / "server");
        if (!started) {
          std::fprintf(stderr, "Server session startup failed: %s\n", session.status().c_str());
          controls.ui.display_menu.status_code = DISPLAY_MENU_STATUS_FAILED;
          game_ui->show_main_menu();
        } else {
          world = requested;
          world_chosen = true;
        }
      }
    }
    auto menu_ui = graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(menu_ui, profile.snapshot());
    SDL_GetWindowSizeInPixels(window, &width, &height);
    const auto menu_stats = graphics::open_world_renderer_stats(renderer);
    game_ui->set_render_resolution(menu_stats.render_width, menu_stats.render_height,
        menu_stats.display_width, menu_stats.display_height);
    game_ui->update(menu_ui, 0, width, height);
    // Black menu backdrop: UI only, no world, player, or sky.
    if (!graphics::open_world_renderer_render_menu(renderer)) {
      std::fprintf(stderr, "Menu frame failed: %s\n", graphics::open_world_renderer_status(renderer));
      controls.running = false;
    } else SDL_Delay(8);
  }
  if (!controls.running) {
    return MenuEnd::Quit;
  }
  controls.ui.session_active = 1;
  game_ui->show_loading("Preparing world");
  return MenuEnd::WorldReady;
}
}
