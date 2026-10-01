#include "MainMenu.h"
#include "OpenWorld.h"
#include "RuntimeControls.h"
#include "Controls.h"
#include "GameUi.h"
#include "LocalSession.h"
#include "MapManifest.h"
#include "WorldRenderer.h"
#include "WorldProfile.h"
#include "UiData.h"
#include "WorldLibraryController.h"
#include "WorldLibraryIo.h"
#include "LoadingScreen.h"
#include "SessionStartup.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>
#include <memory>

namespace octaryn::client::app {
namespace graphics=octaryn::client::rendering;
namespace fs=std::filesystem;
namespace {
bool valid_port(const char* text) {
  if(!text || !*text)return false;
  long port{};
  for(const char* c=text;*c;++c) {
    if(*c<'0' || *c>'9')return false;
    port=port*10+(*c-'0');if(port>65535)return false;
  }
  return port>0;
}
bool valid_address(const char* text) {
  if(!text || !*text)return false;
  for(const char* c=text;*c;++c) {
    const bool valid=(*c>='a' && *c<='z') || (*c>='A' && *c<='Z') ||
        (*c>='0' && *c<='9') || *c=='.' || *c=='-' || *c=='_' || *c==':';
    if(!valid)return false;
  }
  return true;
}
fs::path library_root(const fs::path& root) {
  const auto* override=SDL_getenv("OCTARYN_CLIENT_LIBRARY_ROOT");
  return override && *override?bundle_path(override):root;
}
bool capture_menu(MenuPhase& phase,const std::string& name) {
  const bool visible=phase.ui->world_library_visible(),loading=phase.ui->loading_visible();
  std::printf("world_library_surface visible=%u loading=%u\n",unsigned(visible),unsigned(loading));
  std::fflush(stdout);
  if(!visible || loading)return false;
  const char* override=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
  fs::path path;
  if(override && *override)path=bundle_path(override);
  else {
    char* pref=SDL_GetPrefPath("ZSGStudios","Octaryn");
    if(!pref)return false;
    path=bundle_path(pref)/"ui-captures"/(name+".bmp");SDL_free(pref);
  }
  std::error_code error;fs::create_directories(path.parent_path(),error);
  const auto utf8=path.generic_u8string();
  return !error && graphics::open_world_renderer_capture_ui(phase.renderer,reinterpret_cast<const char*>(utf8.c_str()));
}
void report_library_io(const char* phase) {
  const auto io=world_library_io_stats();
  std::printf("world_library_io phase=%s source_parses=%llu resource_hashes=%llu model_loads=%llu prepared_catalog_reads=%llu\n",phase,
      static_cast<unsigned long long>(io.source_parses),static_cast<unsigned long long>(io.resource_hashes),
      static_cast<unsigned long long>(io.model_loads),static_cast<unsigned long long>(io.prepared_catalog_reads));
  std::fflush(stdout);
}
}
bool menu_boot_requested(const WorldRunOptions& options) {
  if(options.show_worlds || options.play_world_slot)return true;
  if(!options.map_switch_worlds[0].empty() || !options.connect_endpoint.empty())return false;
  if(options.benchmark_settings || options.benchmark_hidden || options.benchmark_seconds>0 || options.frame_limit>0)return false;
  if(!options.capture_ui.empty() || options.show_item_target)return false;
  if(options.show_settings || options.show_menu || options.show_inventory || options.show_creative || options.show_fsr_settings)return false;
  return !(options.validate_ui || options.validate_distance_changes || options.validate_world_items ||
      options.validate_block_actions || options.validate_temporal || options.validate_lighting_motion || options.validate_lighting_edits);
}
fs::path bundle_path(const char* base) {
  if(!base)throw std::runtime_error("Missing application path");
  return fs::path(reinterpret_cast<const char8_t*>(base));
}
fs::path repo_root(const fs::path& bundle) {
  auto path=bundle;if(path.filename().empty())path=path.parent_path();
  const auto root=path.parent_path().parent_path().parent_path().parent_path();
  if(fs::exists(root/"CMakeLists.txt"))return root;
  char* pref=SDL_GetPrefPath("ZSGStudios","Octaryn");
  if(!pref)throw std::runtime_error(SDL_GetError());
  auto fallback=bundle_path(pref);SDL_free(pref);return fallback;
}
fs::path default_world_path(const fs::path& root,const fs::path& bundle) {
  const auto* override=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  if(override && *override)return bundle_path(override);
  WorldLibrary library(library_root(root),bundle);
  std::string error;fs::path world;
  if(!library.refresh(error) || library.entries().empty() || !library.open_world(library.entries().front().id,world,error))
    throw std::runtime_error(error.empty()?"No world is available. Add a GLB or glTF world from the library.":error);
  return world;
}
void seed_inventory_palette(const fs::path& palette,const fs::path& prior) {
  std::error_code error;
  if(fs::exists(palette,error) || !fs::exists(prior,error))return;
  fs::create_directories(palette.parent_path(),error);
  if(!error)fs::copy_file(prior,palette,error);
}
bool consume_menu_world_request(const fs::path& root,runtime_controls& controls,
    fs::path& out_world,std::string& out_endpoint) {
  auto& menu=controls.display_menu;
  const auto action=menu.action_requested;menu.action_requested=DISPLAY_MENU_ACTION_NONE;
  if(action!=DISPLAY_MENU_ACTION_CONNECT_SERVER)return false;
  if(!valid_address(menu.server_address) || !valid_port(menu.server_port)) {
    menu.status_code=DISPLAY_MENU_STATUS_INVALID_SERVER;return false;
  }
  out_world=root/"remote-cache";
  std::error_code error;fs::create_directories(out_world/"client",error);
  if(error){menu.status_code=DISPLAY_MENU_STATUS_FAILED;return false;}
  out_endpoint=std::string(menu.server_address)+":"+menu.server_port;
  menu.status_code=DISPLAY_MENU_STATUS_CONNECTED;return true;
}
MenuEnd run_menu_phase(MenuPhase& phase) {
  auto& controls=*phase.controls;
  auto& ui=*phase.ui;
  const auto& options=*phase.options;
  world_library_reset_io_stats();
  WorldLibraryController library(library_root(phase.root),phase.bundle);
  controls.ui.session_active=0;
  SDL_SetWindowTitle(phase.window,"Octaryn | Worlds");
  ui.show_world_library();
  std::puts("client_main_menu ready=1 waiting_for_world=1");
  fs::path requested;std::string endpoint;
  std::string selected_name,selected_detail;
  std::unique_ptr<SessionStartup> startup;
  bool loading=false,io_reported=false,feedback_applied=phase.feedback.empty();
  bool added=options.add_world_files.empty(),scanned=options.find_world_folder.empty(),captured=false,validated=false;
  unsigned frames{};
  while(controls.running) {
    library.update(ui);
    read_world_controls(phase.window,controls,true);
    if(!controls.running)break;
    bool chosen=false;
    if(!library.busy() && !feedback_applied) {library.message(phase.feedback,phase.feedback_failed);feedback_applied=true;}
    if(!library.busy() && added && scanned && !io_reported) {
      io_reported=true;report_library_io("menu");
    }
    if(!loading && !library.busy() && !added) {
      std::vector<fs::path> files;for(const auto& path:options.add_world_files)files.emplace_back(bundle_path(path.c_str()));
      library.add_files(files);added=true;
    } else if(!loading && !library.busy() && !scanned) {
      library.find_folder(bundle_path(options.find_world_folder.c_str()));scanned=true;
    } else if(!loading && !library.busy() && options.play_world_slot && !phase.autoplay_consumed) {
      phase.autoplay_consumed=true;
      chosen=library.open_index(options.play_world_slot-1,requested);
    }
    WorldLibraryAction action;
    if(ui.take_world_library_action(action)) {
      chosen=library.action(action,phase.window,requested);
      if(chosen)endpoint.clear();
    }
    const auto legacy=controls.ui.display_menu.action_requested;
    if(legacy==DISPLAY_MENU_ACTION_LOAD_WORLD || legacy==DISPLAY_MENU_ACTION_CONNECT_LOCAL) {
      controls.ui.display_menu.action_requested=DISPLAY_MENU_ACTION_NONE;
      chosen=library.open_index(legacy==DISPLAY_MENU_ACTION_CONNECT_LOCAL?0:controls.ui.display_menu.world_slot,requested);
      if(chosen)endpoint.clear();
    } else if(consume_menu_world_request(phase.root,controls.ui,requested,endpoint))chosen=true;
    if(!loading && phase.qualification_rejoin && !library.busy()) {
      phase.qualification_rejoin=false;requested=*phase.world;
      endpoint=phase.active_endpoint?*phase.active_endpoint:options.connect_endpoint;chosen=true;
    }
    if(chosen) {
      loading=true;
      selected_name=library.loading()?library.selected_name():"Connecting to server";
      selected_detail=library.loading()?library.selected_detail():endpoint;
      ui.show_loading(selected_name);
      ui.set_loading_cancelable(true);
      std::printf("world_loading_screen status=shown source_work_dispatched=0\n");std::fflush(stdout);
      if(!present_loading(phase.window,phase.renderer,ui,controls,library.loading()?library.status():"Starting world",selected_detail,true)) {
        library.action({WorldLibraryActionKind::Cancel},phase.window,requested);
        chosen=false;requested.clear();loading=false;ui.show_world_library();
        if(!controls.running)break;
      }
    }
    const bool opened=library.take_opened(requested,selected_name,selected_detail);
    if(opened || (chosen && !library.loading() && !requested.empty())) {
      const char* override=SDL_getenv("OCTARYN_CLIENT_INVENTORY_PATH");
      const auto palette=override && *override?bundle_path(override):requested/"client"/"inventory.json";
      if(!ui.retarget_palette(palette)) {
        library.message("This save's inventory could not be opened. Your previous save was kept.");
        loading=false;ui.show_world_library();
      }
      else {
        *phase.radius=static_cast<unsigned>(controls.ui.render_distance);
        if(present_loading(phase.window,phase.renderer,ui,controls,"Starting world",selected_detail,true)) {
          try {startup=std::make_unique<SessionStartup>(*phase.session,phase.bundle,requested,*phase.radius,endpoint,phase.root/"logs"/"server");}
          catch(const std::exception& error) {library.message(error.what());loading=false;ui.show_world_library();}
        } else {loading=false;ui.show_world_library();}
      }
    }
    if(startup && startup->ready()) {
        const bool started=startup->succeeded(),cancelled=startup->cancelling();
        const auto error=startup->error();
        if(started)startup->accept();
        startup.reset();
        std::printf("world_session_start result=%s server_running=%u\n",cancelled?"cancelled":started?"ready":"failed",unsigned(phase.session->running()));
        std::fflush(stdout);
        if(started) {
          *phase.world=requested;
          if(phase.active_endpoint)*phase.active_endpoint=endpoint;
          controls.ui.session_active=1;
          phase.loading_detail=selected_detail;
          ui.set_loading_cancelable(true);
          return MenuEnd::WorldReady;
        }
        if(!cancelled)library.message("Could not open world: "+error);
        else library.message("World loading canceled.",false);
        loading=false;ui.show_world_library();
    }
    if(loading) {
      if(!startup && !library.loading() && !opened && !chosen) {
        loading=false;ui.show_world_library();
      } else {
        const auto status=startup?(startup->cancelling()?"Stopping...":"Starting world"):library.status();
        if(!present_loading(phase.window,phase.renderer,ui,controls,status,selected_detail,true)) {
          if(startup)startup->cancel();
          else library.action({WorldLibraryActionKind::Cancel},phase.window,requested);
        }
        SDL_Delay(16);continue;
      }
    }
    // Selected work is queued until the next iteration, after its first frame.
    if(!chosen)library.update(ui);
    auto draw=graphics::make_ui_draw_data(controls.ui);
    graphics::populate_ui_profile(draw,phase.profile->snapshot());
    SDL_GetWindowSizeInPixels(phase.window,phase.width,phase.height);
    const auto stats=graphics::open_world_renderer_stats(phase.renderer);
    ui.set_render_resolution(stats.render_width,stats.render_height,stats.display_width,stats.display_height);
    ui.update(draw,0,*phase.width,*phase.height);
    if(!graphics::open_world_renderer_render_menu(phase.renderer))return MenuEnd::Failed;
    ++frames;
    const bool ready=!library.busy() && added && scanned;
    if(ready && options.validate_ui && !validated) {
      validated=true;
      if(!ui.validate_contract())return MenuEnd::Failed;
      ui.show_world_library();
    }
    if(ready && frames>=6 && !options.capture_ui.empty() && !captured) {
      captured=true;if(!capture_menu(phase,options.capture_ui))return MenuEnd::Failed;
    }
    if(options.show_worlds && ready && options.frame_limit>0 && frames>=unsigned(options.frame_limit)) {
      report_library_io("exit");
      return library.failed()?MenuEnd::Failed:MenuEnd::Quit;
    }
    SDL_Delay(options.benchmark_hidden?16:8);
  }
  report_library_io("exit");return MenuEnd::Quit;
}
}
