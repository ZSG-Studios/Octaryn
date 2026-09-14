#include "GameUiState.h"
#include "Menu.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace octaryn::client::app {
namespace {
bool input_event(const SDL_Event& event) {
  return event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_KEY_UP ||
      event.type==SDL_EVENT_TEXT_INPUT || event.type==SDL_EVENT_MOUSE_MOTION ||
      event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP ||
      event.type==SDL_EVENT_MOUSE_WHEEL;
}
}
void GameUi::State::ProcessEvent(Rml::Event& event) {
  auto* target=event.GetTargetElement();
  if (!target) return;
  if(fsr_event(event,target))return;
  if(inventory_pointer(event,target))return;
  if (event.GetType()=="mousedown" && event.GetParameter<int>("button",0)!=1) return;
  if (event.GetType()=="change") {
    const auto id=target->GetId();
    const auto value=event.GetParameter<Rml::String>("value","");
    if (id=="creative-search") {inventory_query=value;creative_dirty=true;return;}
    auto& menu=controls.display_menu;
    if (id=="world-name") std::snprintf(menu.world_name,sizeof(menu.world_name),"%s",value.c_str());
    if (id=="server-address") std::snprintf(menu.server_address,sizeof(menu.server_address),"%s",value.c_str());
    if (id=="server-port") std::snprintf(menu.server_port,sizeof(menu.server_port),"%s",value.c_str());
    const int light=target->GetAttribute<int>("light",-1);
    if (light>=0 && light<4) {
      float number{};
      if (std::sscanf(value.c_str(),"%f",&number)!=1 || !std::isfinite(number)) return;
      float* fields[]={&lighting.values.ambient_strength,&lighting.values.sun_strength,
                       &lighting.values.fog_distance,&lighting.values.skylight_floor};
      const float low[]={.25f,0,64,.05f}, high[]={3,3,2048,.6f};
      *fields[light]=std::clamp(number,low[light],high[light]);
      lighting_settings_sanitize(&lighting.values);
      lighting.save();
      sync_lighting();
    }
    return;
  }
  while (target && !target->HasAttribute("row") && !target->HasAttribute("action"))
    target=target->GetParentNode();
  if (!target) return;
  if(fsr_event(event,target))return;
  if(event.GetType()=="mouseover") {
    if(inventory_open) inventory_hover(target);
    return;
  }
  if(event.GetType()!="click" && event.GetType()!="mousedown")return;
  if (event.GetType()=="mousedown") {
    const int row=target->GetAttribute<int>("row",-1);
    if (lighting.visible || controls.display_menu.screen!=DISPLAY_MENU_SCREEN_SETTINGS || row<0 || row>=12) return;
  }
  const auto action=target->GetAttribute<Rml::String>("action","");
  if(action=="cycle-upscaler") {
    if(event.GetType()=="click")controls.display_menu.upscaler_mode=(controls.display_menu.upscaler_mode+1)%7;
  }
  else if(action=="toggle-ray-tracing") {
    if(event.GetType()=="click" && controls.ray_tracing_available && !target->HasAttribute("disabled"))
      controls.display_menu.ray_tracing_enabled=controls.display_menu.ray_tracing_enabled?0:1;
  }
  else if (action=="close-lighting") open_pause();
  else if (event.GetType()=="click" && inventory_action(target,action)) {}
  else if (target->HasAttribute("row")) {
    const bool return_to_pause=controls.display_menu.screen==DISPLAY_MENU_SCREEN_SETTINGS &&
        (target->GetAttribute<int>("row",-1)==DISPLAY_MENU_APPLY_ROW ||
         target->GetAttribute<int>("row",-1)==DISPLAY_MENU_CLOSE_ROW);
    pending|=runtime_controls_activate_menu_row(&controls,window,target->GetAttribute<int>("row",-1),
                                               event.GetType()=="mousedown"?-1:target->GetAttribute<int>("delta",1));
    if(return_to_pause && controls.session_active) open_pause();
  }
  sync_menu();
  sync_capture();
}
std::uint32_t GameUi::event(const SDL_Event& input,int width,int height) {
  auto& s=*state_;
  const auto finish=[&](std::uint32_t flags) {s.release_input();return flags;};
  s.context->SetDimensions({std::max(width,1),std::max(height,1)});
  SDL_Event event=input;
  if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) {
    s.mouse_was_relative=false;
    s.controls.restore_relative_mouse_after_ui=0;
    s.release_input_pending=true;
    s.release_input();
  }
  const bool pressed=event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat;
  const auto* focused=s.context->GetFocusElement();
  const bool typing=s.modal_open() && focused && focused->GetTagName()=="input";
  if (pressed && !typing && (event.key.key==SDLK_I || event.key.key==SDLK_E || event.key.key==SDLK_B) &&
      (!s.modal_open() || s.inventory_open)) {
    const bool creative=event.key.key==SDLK_B;
    if (s.inventory_open && creative==s.creative_open) s.close_inventory();
    else s.open_inventory(creative);
    s.context->Update();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && s.fsr_open) {
    s.fsr_open=false;s.sync_menu();s.context->Update();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && s.inventory_open) {
    s.close_inventory();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && !s.modal_open()) {
    s.open_pause();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED|RUNTIME_CONTROLS_MENU_OPENED);
  }
  if (pressed && event.key.key==SDLK_F6) {
    if(s.inventory_open) {if(!s.drop_request.count)s.inventory.cancel_move();s.inventory_open=false;}
    s.lighting.visible=!s.lighting.visible;
    s.sync_menu();s.sync_lighting();s.sync_capture();
    s.context->Update();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if (pressed && event.key.key==SDLK_ESCAPE && s.lighting.visible) {
    s.open_pause();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  const bool global_key=pressed && (event.key.key==SDLK_F11 || event.key.key==SDLK_F3);
  if(pressed && !typing && event.key.key==SDLK_T && (!s.modal_open() || s.inventory_open)) {
    s.request_drop((event.key.mod&SDL_KMOD_CTRL)!=0);s.sync_inventory();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if (global_key || !s.modal_open()) {
    auto flags=runtime_controls_handle_event(&s.controls,s.window,&event,width,height);
    if (!flags && !global_key) return finish(0);
    s.sync_menu();s.sync_capture();
    if (flags & RUNTIME_CONTROLS_EVENT_CAPTURED) {s.context->Update();return finish(flags);}
  }
  if (s.modal_open()) {
    if (pressed && s.controls.display_menu.active &&
        (event.key.key==SDLK_LEFT || event.key.key==SDLK_RIGHT)) {
      auto* focused=s.context->GetFocusElement();
      const int row=focused?focused->GetAttribute<int>("row",-1):-1;
      if (row>=0 && row<12 && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_SETTINGS) {
        const auto flags=runtime_controls_activate_menu_row(&s.controls,s.window,row,event.key.key==SDLK_LEFT?-1:1);
        s.sync_menu();s.context->Update();return finish(flags);
      }
    }
    if (pressed && event.key.key==SDLK_ESCAPE) {
      auto& menu=s.controls.display_menu;
      std::uint32_t flags=RUNTIME_CONTROLS_EVENT_CAPTURED;
      if(s.controls_open) s.open_pause();
      else if(menu.screen==DISPLAY_MENU_SCREEN_SETTINGS) {
        flags|=runtime_controls_close_menu(&s.controls,s.window);
        if(s.controls.session_active) s.open_pause();
      } else if (menu.screen==DISPLAY_MENU_SCREEN_INGAME)
        flags|=runtime_controls_close_menu(&s.controls,s.window);
      else {menu.screen=DISPLAY_MENU_SCREEN_MAIN;menu.row=2;}
      s.sync_menu();s.sync_capture();return finish(flags);
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP) {
      const float density=SDL_GetWindowPixelDensity(s.window);
      s.context->ProcessMouseMove(int(event.button.x*density),int(event.button.y*density),RmlSDL::GetKeyModifierState());
    }
    RmlSDL::InputEventHandler(s.context,s.window,event);
    // Pointer movement changes only retained cursor/hover state. Layout and
    // geometry are refreshed once by update(), not at mouse polling frequency.
    if(event.type==SDL_EVENT_MOUSE_MOTION)return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
    s.sync_inventory();s.sync_menu();s.sync_capture();
    s.context->Update();
    const auto flags=s.pending|(input_event(event)?RUNTIME_CONTROLS_EVENT_CAPTURED:0u);
    s.pending=0;
    return finish(flags);
  }
  return finish(0);
}
}
