#pragma once
#include "Menu.h"
#include "RenderDistance.h"
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::app {
// Domain-level qualification: use the same Apply handler as the settings menu.
class DistanceValidation {
  unsigned phase_{},resident_frames_{};
  unsigned previous_columns_{~0u};
  Uint64 last_progress_{};
  bool complete_{};
public:
  bool complete() const {return complete_;}
  bool advance(SDL_Window* window,runtime_controls& controls,unsigned radius,unsigned columns,bool rendered) {
    const auto now=SDL_GetTicks();
    if(columns!=previous_columns_) {previous_columns_=columns;last_progress_=now;}
    if(now-last_progress_>60000) throw std::runtime_error("Render distance qualification stalled");
    const unsigned expected=phase_==1?8u:4u;
    if(radius!=expected) throw std::runtime_error("Render distance qualification applied the wrong radius");
    if(!rendered) {resident_frames_=0;return false;}
    if(columns!=(2*radius+1)*(2*radius+1)) {resident_frames_=0;return false;}
    if(++resident_frames_<30) return false;
    std::printf("render_distance_validation phase=%u radius=%u resident=%u\n",phase_,radius,columns);
    std::fflush(stdout);
    if(phase_==2) {complete_=true;std::puts("render_distance_changes=passed grow=8 shrink=4 os_events_injected=0");return true;}
    const int distance=phase_==0?8:4;
    const auto saved_menu=controls.display_menu;
    controls.display_menu.display_dirty=0;
    for(int i=0;i<render_distance_option_count();++i)
      if(render_distance_options()[i]==distance)controls.display_menu.render_distance_index=i;
    const auto result=runtime_controls_request_apply(&controls,window);
    controls.display_menu=saved_menu;
    if(!(result&RUNTIME_CONTROLS_MENU_APPLIED) || controls.render_distance!=distance)
      throw std::runtime_error("Render distance Apply qualification failed");
    ++phase_;resident_frames_=0;last_progress_=now;
    return false;
  }
};
}
