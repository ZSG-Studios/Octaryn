#include "GameUiState.h"
#include "Menu.h"
#include "RuntimeSettings.h"
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
    if (light>=0 && light<3) {
      float number{};char trailing{};
      if (std::sscanf(value.c_str(),"%f %c",&number,&trailing)!=1 || !std::isfinite(number)) return;
      float* fields[]={&lighting.values.ambient_strength,&lighting.values.sun_strength,
                       &lighting.values.fog_distance};
      const float low[]={.25f,0,64}, high[]={3,3,2048};
      *fields[light]=std::clamp(number,low[light],high[light]);
      lighting_settings_sanitize(&lighting.values);
      lighting.save();
      sync_lighting();
    }
    const int range=target->GetAttribute<int>("range",-1);
    if (range>=0 && range<2) {
      float number{};char trailing{};
      if (std::sscanf(value.c_str(),"%f %c",&number,&trailing)!=1 || !std::isfinite(number)) return;
      auto& display=controls.display_menu;
      uint16_t* fields[]={&display.shadow_distance,&display.reflection_distance};
      const float high[]={1024,1024};
      *fields[range]=static_cast<uint16_t>(std::lround(std::clamp(number,0.f,high[range])));
      sync_menu();
    }
    // F6 panel rows apply immediately to the live controls and persist.
    const int live=target->GetAttribute<int>("live",-1);
    if (live>=0 && live<2) {
      float number{};char trailing{};
      if (std::sscanf(value.c_str(),"%f %c",&number,&trailing)!=1 || !std::isfinite(number)) return;
      uint16_t* fields[]={&controls.shadow_distance,&controls.reflection_distance};
      const float high[]={1024,1024};
      *fields[live]=static_cast<uint16_t>(std::lround(std::clamp(number,0.f,high[live])));
      uint16_t* staged[]={&menu.shadow_distance,&menu.reflection_distance};
      *staged[live]=*fields[live];
      runtime_settings_save(window,&controls);
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
    if(!target->HasAttribute("disabled"))audio_feedback.transition(true);
  }
  const auto action=target->GetAttribute<Rml::String>("action","");
  if(action=="loading-cancel") {
    if(event.GetType()=="click" && loading_visible && loading_cancelable) {
      loading_cancel_requested=loading_cancelling=true;loading_cancelable=false;
      loading_status="Cancelling...";loading_fraction=-1;sync_loading();
    }
    return;
  }
  if(loading_visible)return;
  if(action=="cycle-upscaler") {
    if(event.GetType()=="click")controls.display_menu.upscaler_mode=(controls.display_menu.upscaler_mode+1)%7;
  }
  else if(action=="cycle-vsync") {
    if(event.GetType()=="click")
      controls.display_menu.present_mode_index=(controls.display_menu.present_mode_index+1)%DISPLAY_MENU_PRESENT_MODE_COUNT;
  }
  else if(action=="cycle-frame-cap") {
    if(event.GetType()=="click") {
      constexpr uint16_t caps[]={0,30,60,120,144,165,240,1};
      unsigned index=0;
      while(index<8 && caps[index]!=controls.display_menu.frame_cap_fps) ++index;
      controls.display_menu.frame_cap_fps=caps[index>=8?1:(index+1)%8];
    }
  }
  else if(action=="toggle-ray-tracing") {
    if(event.GetType()=="click" && controls.ray_tracing_available && !target->HasAttribute("disabled"))
      controls.display_menu.ray_tracing_enabled=controls.display_menu.ray_tracing_enabled?0:1;
  }
  else if(action=="toggle-atmo") {
    if(event.GetType()=="click") {
      const int atmo=target->GetAttribute<int>("atmo",-1);
      uint8_t* fields[]={&controls.fog_enabled,&controls.clouds_enabled,&controls.sky_gradient_enabled,
        &controls.stars_enabled,&controls.sun_enabled,&controls.moon_enabled};
      if(atmo>=0 && atmo<6) {
        *fields[atmo]=*fields[atmo]?0:1;
        // Keep the staged settings copy in sync so a later Apply cannot
        // resurrect stale values over the live toggle.
        uint8_t* staged[]={&controls.display_menu.fog_enabled,&controls.display_menu.clouds_enabled,
          &controls.display_menu.sky_gradient_enabled,&controls.display_menu.stars_enabled,
          &controls.display_menu.sun_enabled,&controls.display_menu.moon_enabled};
        *staged[atmo]=*fields[atmo];
        runtime_settings_save(window,&controls);sync_lighting();
      }
    }
  }
  else if(action=="toggle-ray-tracing-live") {
    if(event.GetType()=="click" && controls.ray_tracing_available) {
      controls.ray_tracing_enabled=controls.ray_tracing_enabled?0:1;
      controls.display_menu.ray_tracing_enabled=controls.ray_tracing_enabled;
      runtime_settings_save(window,&controls);sync_lighting();
    }
  }
  else if(action=="cycle-reflection-quality" || action=="cycle-shadow-quality") {
    if(event.GetType()=="click") {
      auto& quality=action=="cycle-reflection-quality"?controls.reflection_quality:controls.shadow_quality;
      quality=(quality+1)%4;
      runtime_settings_save(window,&controls);sync_lighting();
    }
  }
  else if(action=="cycle-lighting-debug") {
    if(event.GetType()=="click") {lighting.debug_view=next_lighting_debug(lighting.debug_view);sync_lighting();}
  }
  else if (action=="close-lighting") return_to_menu();
  else if (event.GetType()=="click" && inventory_action(target,action)) {}
  else if (target->HasAttribute("row")) {
    const bool finished_settings=controls.display_menu.screen==DISPLAY_MENU_SCREEN_SETTINGS &&
        (target->GetAttribute<int>("row",-1)==DISPLAY_MENU_APPLY_ROW ||
         target->GetAttribute<int>("row",-1)==DISPLAY_MENU_CLOSE_ROW);
    pending|=runtime_controls_activate_menu_row(&controls,window,target->GetAttribute<int>("row",-1),
                                               event.GetType()=="mousedown"?-1:target->GetAttribute<int>("delta",1));
    if(finished_settings) return_to_menu();
  }
  sync_menu();
  sync_capture();
}
std::uint32_t GameUi::event(const SDL_Event& input,int width,int height) {
  auto& s=*state_;
  s.audio_feedback.block_inventory(s.drop_request.count!=0);
  const bool audio_motion=input.type==SDL_EVENT_MOUSE_MOTION && s.audio_feedback.pointer_moved(input.motion.x,input.motion.y);
  ui::UiAudioFeedback::Input audio_input(s.audio_feedback,input_event(input) &&
      !(input.type==SDL_EVENT_KEY_DOWN && input.key.repeat),audio_motion,
      input.type==SDL_EVENT_KEY_DOWN,s.system.GetElapsedTime());
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
  if(s.loading_visible && input_event(event)) {
    if(pressed && event.key.key==SDLK_ESCAPE) {
      if(s.loading_cancelable) {
        if(auto* cancel=s.document->GetElementById("loading-cancel")) {
          s.audio_feedback.activate(cancel);cancel->DispatchEvent("click",{});
        }
      }
    } else RmlSDL::InputEventHandler(s.context,s.window,event);
    audio_input.stop();s.sync_loading();s.context->Update();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(s.world_library_key(event))return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  if(s.module_panel && pressed && (event.key.key==SDLK_F8 || (event.key.key==SDLK_ESCAPE && s.module_panel->visible()))) {
    s.module_panel->toggle();s.sync_capture();s.context->Update();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  const auto* focused=s.context->GetFocusElement();
  const bool typing=s.modal_open() && focused && focused->GetTagName()=="input";
  if (s.module_actions_enabled && pressed && !s.modal_open()) {
    std::string action;
    if (event.key.key==SDLK_T)
      action=(event.key.mod&SDL_KMOD_CTRL)?"inventory.drop_stack":"inventory.drop";
    else if (event.key.key==SDLK_G) action="interact.use";
    else if (event.key.key>=SDLK_1 && event.key.key<=SDLK_9)
      action="inventory.select."+std::to_string(event.key.key-SDLK_1);
    else if (event.key.key==SDLK_0) action="inventory.select.9";
    if (!action.empty()) {
      queue_module_action(action);
      return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
    }
  }
  if (pressed && !typing && (event.key.key==SDLK_I || event.key.key==SDLK_E || event.key.key==SDLK_B) &&
      (!s.modal_open() || s.inventory_open)) {
    const bool creative=event.key.key==SDLK_B;
    s.audio_feedback.transition();audio_input.stop();
    if (s.inventory_open && creative==s.creative_open) s.close_inventory();
    else s.open_inventory(creative);
    s.context->Update();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && s.fsr_open) {
    s.audio_feedback.transition();audio_input.stop();
    s.fsr_open=false;s.sync_menu();s.context->Update();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && s.inventory_open) {
    s.audio_feedback.transition();audio_input.stop();
    s.close_inventory();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if(pressed && event.key.key==SDLK_ESCAPE && !s.modal_open()) {
    s.audio_feedback.transition();audio_input.stop();
    s.open_pause();return finish(RUNTIME_CONTROLS_EVENT_CAPTURED|RUNTIME_CONTROLS_MENU_OPENED);
  }
  if (pressed && event.key.key==SDLK_F6) {
    s.audio_feedback.transition();audio_input.stop();
    if(s.inventory_open) {if(!s.drop_request.count)s.inventory.cancel_move();s.inventory_open=false;}
    s.lighting.visible=!s.lighting.visible;
    s.sync_menu();s.sync_lighting();s.sync_capture();
    s.context->Update();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if (pressed && event.key.key==SDLK_ESCAPE && s.lighting.visible) {
    s.audio_feedback.transition();audio_input.stop();
    s.open_pause();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  if (pressed && event.key.key==SDLK_F7) {
    s.ui_capture_requested=true;
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  const bool global_key=pressed && (event.key.key==SDLK_F11 || event.key.key==SDLK_F3);
  if(pressed && !typing && event.key.key==SDLK_T && (!s.modal_open() || s.inventory_open)) {
    s.request_drop((event.key.mod&SDL_KMOD_CTRL)!=0);s.sync_inventory();
    return finish(RUNTIME_CONTROLS_EVENT_CAPTURED);
  }
  // Interact: pick up the highlighted world item (or use the look target).
  if(pressed && !typing && event.key.key==SDLK_G && !s.modal_open()) {
    s.interact_requested=true;
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
      auto* row_focused=s.context->GetFocusElement();
      const int row=row_focused?row_focused->GetAttribute<int>("row",-1):-1;
      if (row>=0 && row<12 && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_SETTINGS) {
        const auto flags=runtime_controls_activate_menu_row(&s.controls,s.window,row,event.key.key==SDLK_LEFT?-1:1);
        s.audio_feedback.transition(true);audio_input.stop();
        s.sync_menu();s.context->Update();return finish(flags);
      }
    }
    if (pressed && event.key.key==SDLK_ESCAPE) {
      s.audio_feedback.transition();audio_input.stop();
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
    audio_input.stop();
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
