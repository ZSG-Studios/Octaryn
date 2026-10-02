#include "LoadingScreen.h"
#include "LoadingPresentation.h"
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
#include <string_view>

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
  const auto frame_started=SDL_GetTicksNS();
  ui.update(rendering::make_ui_draw_data(controls.ui),0,width,height);
  if(!rendering::open_world_renderer_render_menu(renderer))
    throw std::runtime_error("Loading screen presentation failed");
  const auto presented=SDL_GetTicksNS();
  auto& observation=loading_presentation();
  if(!observation.started)observation.started=presented;
  ++observation.frames;
  bool captured=false;
  if(const auto* directory=SDL_getenv("OCTARYN_CLIENT_LOADING_CAPTURE_DIR");directory && *directory) {
    const auto* interval_setting=SDL_getenv("OCTARYN_CLIENT_LOADING_CAPTURE_INTERVAL_MS");
    const unsigned interval=interval_setting?unsigned(std::clamp(SDL_atoi(interval_setting),50,60000)):0;
    const auto* count_setting=SDL_getenv("OCTARYN_CLIENT_LOADING_CAPTURE_COUNT");
    const unsigned limit=count_setting?unsigned(std::clamp(SDL_atoi(count_setting),1,256)):64;
    // Default captures each stage once per request; the interval mode records
    // animation over time instead of resetting its sampling at every stage.
    const bool requested=interval?observation.captures<limit &&
        (!observation.captures || presented-observation.last_capture>=std::uint64_t(interval)*1000000):
        !observation.stages.contains(stage);
    if(requested) {
      std::string name;
      for(const unsigned char value:stage)name+=std::isalnum(value)?char(value):'-';
      if(interval)name="sample-"+std::to_string(observation.captures)+"-"+name;
      const auto folder=bundle_path(directory)/("loading-"+std::to_string(observation.epoch));
      std::filesystem::create_directories(folder);
      const auto file=(folder/(name+".bmp")).generic_u8string();
      if(!rendering::open_world_renderer_capture_ui(renderer,reinterpret_cast<const char*>(file.c_str()),true))
        throw std::runtime_error("Loading screen capture failed");
      observation.stages.insert(stage);observation.last_capture=presented;++observation.captures;captured=true;
      std::printf("loading_capture epoch=%llu frame=%llu elapsed_ms=%.3f stage=%s viewport=%dx%d sample=%u\n",
          static_cast<unsigned long long>(observation.epoch),static_cast<unsigned long long>(observation.frames),
          double(presented-observation.started)/1e6,stage.c_str(),width,height,observation.captures);
    }
  }
  const auto finished=SDL_GetTicksNS();
  if(const auto* trace=SDL_getenv("OCTARYN_CLIENT_LOADING_TRACE");trace && std::string_view(trace)=="1") {
    std::printf("loading_frame epoch=%llu frame=%llu elapsed_ms=%.3f gap_ms=%.3f draw_ms=%.3f capture_ms=%.3f captured=%u stage=%s\n",
        static_cast<unsigned long long>(observation.epoch),static_cast<unsigned long long>(observation.frames),
        double(presented-observation.started)/1e6,observation.last?double(presented-observation.last)/1e6:0.,
        double(presented-frame_started)/1e6,captured?double(finished-presented)/1e6:0.,unsigned(captured),stage.c_str());
    std::fflush(stdout);
  }
  observation.last=presented;
  const bool validation_cancel=validate_loading_cancel(window,ui,stage);
  return controls.running && !cancelled && !(validation_cancel && ui.take_loading_cancel());
}
}
