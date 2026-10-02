#include "MainMenu.h"
#include "OpenWorld.h"
#include "WorldLibraryController.h"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace octaryn::client::app {
namespace fs=std::filesystem;
bool menu_boot_requested(const WorldRunOptions& options) {
  if(options.startup_menu)return true;
  return !options.frame_limit && options.benchmark_seconds<=0 && !options.benchmark_hidden &&
      !options.play_world_slot && options.connect_endpoint.empty() &&
      !options.validate_session_rejoin && !options.validate_ui && !options.validate_temporal &&
      !options.validate_module_actions && !options.validate_frame_pacing &&
      !options.validate_distance_changes && !options.validate_world_items && !options.validate_block_actions &&
      !options.validate_lighting_motion && !options.validate_lighting_edits && options.map_switch_worlds[0].empty() &&
      options.capture_ui.empty() && !options.show_settings && !options.show_fsr_settings &&
      !options.show_inventory && !options.show_creative && !options.show_lighting && !options.show_item_target;
}
fs::path bundle_path(const char* value) {
  if(!value)throw std::runtime_error("Missing application path");
  return fs::path(reinterpret_cast<const char8_t*>(value));
}
fs::path repo_root(const fs::path& bundle) {
  auto path=bundle;if(path.filename().empty())path=path.parent_path();
  const auto root=path.parent_path().parent_path().parent_path().parent_path();
  if(fs::exists(root/"CMakeLists.txt"))return root;
  char* pref=SDL_GetPrefPath("ZSGStudios","Octaryn");
  if(!pref)throw std::runtime_error(SDL_GetError());
  auto result=bundle_path(pref);SDL_free(pref);return result;
}
fs::path default_world_path(const fs::path& root,const fs::path& bundle) {
  const auto* override=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  if(override && *override)return bundle_path(override);
  // This is a world source lookup, not an implicit game menu or scene import.
  const auto* library_override=SDL_getenv("OCTARYN_CLIENT_LIBRARY_ROOT");
  WorldLibrary library(library_override && *library_override?bundle_path(library_override):root,bundle);
  std::string error;fs::path world;
  if(!library.refresh(error) || library.entries().empty() || !library.open_world(library.entries().front().id,world,error))
    throw std::runtime_error(error.empty()?"Selected game has no configured startup world.":error);
  return world;
}
void seed_inventory_palette(const fs::path&,const fs::path&) {}
bool consume_menu_world_request(const fs::path&,runtime_controls&,fs::path&,std::string&) {return false;}
MenuEnd run_menu_phase(MenuPhase&) {return MenuEnd::Failed;}
}
