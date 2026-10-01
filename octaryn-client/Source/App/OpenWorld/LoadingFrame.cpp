#include "LoadingScreen.h"
#include "GameUi.h"
#include "Controls.h"
#include "WorldRenderer.h"
#include "UiData.h"
#include "MainMenu.h"
#include <SDL3/SDL.h>
#include <cctype>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace octaryn::client::app {
bool validate_loading_cancel(SDL_Window* window,GameUi& ui,const std::string& stage) {
  const auto* target=SDL_getenv("OCTARYN_CLIENT_LOADING_CANCEL_STAGE");
  if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN) || !target || stage!=target)return false;
  static unsigned occurrence{};
  const auto* count=SDL_getenv("OCTARYN_CLIENT_LOADING_CANCEL_OCCURRENCE");
  const unsigned requested=count?unsigned(std::max(1,SDL_atoi(count))):1;
  if(++occurrence!=requested || !ui.validation_cancel_loading())return false;
  std::printf("world_loading_validation_cancel stage=%s occurrence=%u\n",stage.c_str(),occurrence);
  std::fflush(stdout);return true;
}
bool present_loading(SDL_Window* window,rendering::WorldRenderer* renderer,GameUi& ui,
    WorldControls& controls,const std::string& stage,const std::string& detail,bool cancelable,bool draw) {
  if(!draw) {
    int width{},height{};SDL_GetWindowSizeInPixels(window,&width,&height);
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID==SDL_GetWindowID(window)))controls.running=false;
      ui.loading_event(event,width,height);
    }
    return controls.running && !ui.take_loading_cancel();
  }
  read_world_controls(window,controls,true);
  const bool cancelled=ui.take_loading_cancel();
  ui.set_loading_cancelable(cancelable && !cancelled);
  ui.update_loading(cancelled?"Stopping...":stage,detail,-1);
  int width{},height{};
  SDL_GetWindowSizeInPixels(window,&width,&height);
  const auto stats=rendering::open_world_renderer_stats(renderer);
  ui.set_render_resolution(stats.render_width,stats.render_height,stats.display_width,stats.display_height);
  ui.update(rendering::make_ui_draw_data(controls.ui),0,width,height);
  if(!rendering::open_world_renderer_render_menu(renderer))
    throw std::runtime_error("Loading screen presentation failed");
  if(const auto* directory=SDL_getenv("OCTARYN_CLIENT_LOADING_CAPTURE_DIR");directory && *directory) {
    static std::unordered_map<std::string,bool> captured;
    const auto key=std::string(directory)+stage;
    if(!captured[key]) {
      captured[key]=true;
      std::string name;
      for(const unsigned char value:stage)name+=std::isalnum(value)?char(value):'-';
      const auto folder=bundle_path(directory);
      std::filesystem::create_directories(folder);
      const auto file=(folder/(name+".bmp")).generic_u8string();
      if(!rendering::open_world_renderer_capture_ui(renderer,reinterpret_cast<const char*>(file.c_str())))
        throw std::runtime_error("Loading screen capture failed");
    }
  }
  const bool validation_cancel=validate_loading_cancel(window,ui,stage);
  return controls.running && !cancelled && !(validation_cancel && ui.take_loading_cancel());
}
}
