#include "GameUiState.h"
#include "RenderDistance.h"
#include "Menu.h"
#include <RmlUi/Core/ComputedValues.h>
#include <array>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
namespace {
constexpr std::array setting_ids={"display","resolution","fullscreen","distance","fog","clouds",
    "sky","stars","sun","moon","pom","pbr","vsync","frame-cap"};
constexpr std::array light_ids={"ambient","sun-strength","fog-distance","sky-floor"};
constexpr std::array light_min={.25f,0.f,64.f,.05f};
constexpr std::array light_max={3.f,3.f,2048.f,.6f};
constexpr std::array light_step={.01f,.01f,1.f,.01f};
std::array<int,12> settings(const display_menu& menu) {
  return {menu.display_index,menu.mode_index,menu.fullscreen,menu.render_distance_index,
    menu.fog_enabled,menu.clouds_enabled,menu.sky_gradient_enabled,menu.stars_enabled,
    menu.sun_enabled,menu.moon_enabled,menu.pom_enabled,menu.pbr_enabled};
}
}

bool GameUi::validate_contract() {
  auto& s=*state_;
  unsigned checks{},failures{};
  auto expect=[&](bool valid,const char* requirement,const char* id) {
    ++checks;
    if (!valid) {
      ++failures;
      std::fprintf(stderr,"rml_ui_check=failed requirement=%s element=%s\n",requirement,id);
    }
  };
  if (!s.document || !s.context) {
    std::fprintf(stderr,"rml_ui_contract=failed reason=missing_document_or_context\n");
    return false;
  }
  auto element=[&](const char* id) {
    auto* result=s.document->GetElementById(id);
    expect(result!=nullptr,"required_id",id);
    return result;
  };
  constexpr std::array required={"hud","crosshair","selected-block","diagnostics","fps","metrics","samples",
    "scrim","menu","settings-screen","main-screen","pause-screen","worlds-screen","servers-screen",
    "apply","menu-status","world-name","server-address","server-port",
    "world-0","world-1","world-2","world-0-state","world-1-state","world-2-state","delete-confirm",
    "lighting","close-lighting","lighting-debug","lighting-debug-value",
    "live-raster-sun","live-raster-sun-value",
    "live-gi-voxel","live-gi-voxel-number",
    "live-shadow-distance","live-shadow-distance-number","live-reflection-distance","live-reflection-distance-number",
    "live-ray-tracing","live-ray-tracing-value","lighting-quality","lighting-quality-value",
    "live-fog","live-clouds","live-sky","live-stars","live-sun","live-moon"};
  for (const auto* id:required) element(id);
  constexpr std::array range_ids={"live-gi-voxel","live-shadow-distance","live-reflection-distance"};
  for (const auto* id:range_ids) {
    if (auto* slider=element(id))
      expect(slider->GetTagName()=="input" && slider->GetAttribute<Rml::String>("type","")=="range" &&
          slider->GetAttribute<int>("live",-1)>=0,"range_slider_binding",id);
    element((std::string(id)+"-value").c_str());
    if (auto* number=element((std::string(id)+"-number").c_str()))
      expect(number->GetTagName()=="input","range_number_binding",id);
  }
  for (std::size_t row=0;row<setting_ids.size();++row) {
    const auto* id=setting_ids[row];
    if(row>=12) {
      constexpr const char* actions[]={"cycle-vsync","cycle-frame-cap"};
      const char* action=actions[row-12];
      if(auto* button=element(id))
        expect(button->GetTagName()=="button" && button->GetAttribute<Rml::String>("action","")==action &&
            !button->HasAttribute("row"),"graphics_action_binding",id);
      element((std::string(id)+"-value").c_str());
      continue; // Graphics actions preserve the numbered historical rows.
    }
    // Atmosphere rows 4-9 moved to the live F6 panel; only their adjust
    // semantics are validated here, not settings-screen markup.
    const bool in_settings_screen=row<4 || row>9;
    if (in_settings_screen) {
      if (auto* button=element(id)) {
        expect(button->GetTagName()=="button","setting_button",id);
        expect(button->GetAttribute<int>("row",-1)==static_cast<int>(row),"setting_row",id);
      }
      element((std::string(id)+"-value").c_str());
    }
    display_menu menu{};
    menu.screen=DISPLAY_MENU_SCREEN_SETTINGS;
    menu.display_count=menu.mode_count=3;
    menu.row=static_cast<int>(row);
    const auto before=settings(menu);
    display_menu_adjust(&menu,1,4);
    const auto after=settings(menu);
    for (std::size_t field=0;field<before.size();++field)
      expect(after[field]==before[field]+(field==row?1:0),"setting_semantics",id);
    display_menu_adjust(&menu,-1,4);
    expect(settings(menu)==before,"setting_reversible",id);
  }
  for (std::size_t index=0;index<light_ids.size();++index) {
    const auto* id=light_ids[index];
    if (auto* slider=element(id)) {
      expect(slider->GetTagName()=="input" && slider->GetAttribute<Rml::String>("type","")=="range",
             "lighting_range",id);
      expect(slider->GetAttribute<int>("light",-1)==static_cast<int>(index),"lighting_binding",id);
      expect(std::abs(slider->GetAttribute<float>("min",-1)-light_min[index])<.0001f,"lighting_min",id);
      expect(std::abs(slider->GetAttribute<float>("max",-1)-light_max[index])<.0001f,"lighting_max",id);
      expect(std::abs(slider->GetAttribute<float>("step",-1)-light_step[index])<.0001f,"lighting_step",id);
    }
    const auto number=std::string(id)+"-number";
    if (auto* input=element(number.c_str()))
      expect(input->GetAttribute<int>("light",-1)==static_cast<int>(index),"lighting_number_binding",number.c_str());
    element((std::string(id)+"-value").c_str());
  }

  {
    const auto original=s.controls;const auto pending=s.pending;
    s.controls.ray_tracing_available=1;s.controls.ray_tracing_enabled=1;
    s.lighting.visible=1;s.sync_lighting();
    if(auto* button=element("live-ray-tracing")) {
      button->DispatchEvent("click",{});
      expect(s.controls.ray_tracing_enabled==0,"ray_live_toggle_off","live-ray-tracing");
      button->DispatchEvent("click",{});
      expect(s.controls.ray_tracing_enabled==1,"ray_live_toggle_on","live-ray-tracing");
    }
    s.controls=original;s.pending=pending;s.lighting.visible=0;s.sync_menu();
  }
  const auto original_menu=s.controls.display_menu;
  const auto original_distance=s.controls.render_distance;
  constexpr std::array distances={4,8,12,16,20,24,32};
  expect(render_distance_option_count()==distances.size(),"original_distance_count","distance");
  s.controls.display_menu.screen=DISPLAY_MENU_SCREEN_SETTINGS;
  s.controls.display_menu.row=3;
  s.controls.display_menu.render_distance_index=0;
  s.controls.display_menu.display_dirty=0;
  for (std::size_t i=0;i<distances.size();++i) {
    expect(static_cast<std::size_t>(s.controls.display_menu.render_distance_index)==i,"distance_cycle_index","distance");
    expect(render_distance_options()[i]==distances[i],"original_distance_choice","distance");
    runtime_controls_request_apply(&s.controls,s.window);
    expect(s.controls.render_distance==distances[i],"distance_apply","distance");
    s.controls.display_menu.row=3;
    runtime_controls_adjust_menu(&s.controls,s.window,1);
  }
  expect(s.controls.display_menu.render_distance_index==0,"distance_cycle_wrap","distance");
  s.controls.render_distance=original_distance;
  s.controls.display_menu=original_menu;
  const bool original_lighting=s.lighting.visible;
  const auto original_dimensions=s.context->GetDimensions();
  const auto original_density=s.context->GetDensityIndependentPixelRatio();
  const bool original_compact=s.document->IsClassSet("compact");
  const auto original_debug=s.controls.debug_overlay_enabled;
  const bool original_modal=s.modal_was_open,original_lighting_was=s.lighting_was_visible;
  const bool original_release=s.release_input_pending;
  const auto original_screen=s.previous_screen;
  s.modal_was_open=false;s.lighting_was_visible=false;s.release_input_pending=false;
  s.controls.display_menu.active=0;s.controls.display_menu.status_code=0;s.lighting.visible=false;
  s.sync_menu();
  if (auto* status=s.document->GetElementById("menu-status")) {
    const auto before=status->GetInnerRML();
    // A model change is synchronized by update/global actions, not ordinary gameplay input.
    s.controls.display_menu.status_code=14;
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.xrel=1.f;
    const auto started=SDL_GetTicksNS();
    std::uint32_t motion_flags{};
    for (unsigned i=0;i<8192;++i)motion_flags|=event(motion,original_dimensions.x,original_dimensions.y);
    const auto elapsed=SDL_GetTicksNS()-started;
    expect(motion_flags==0,"gameplay_motion_uncaptured","input");
    expect(status->GetInnerRML()==before,"gameplay_motion_skips_menu_sync","input");
    SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.key=SDLK_W;key.key.scancode=SDL_SCANCODE_W;
    expect(event(key,original_dimensions.x,original_dimensions.y)==0,"gameplay_key_uncaptured","input");
    expect(status->GetInnerRML()==before,"gameplay_key_skips_menu_sync","input");
    key.key.key=SDLK_F3;key.key.scancode=SDL_SCANCODE_F3;
    expect((event(key,original_dimensions.x,original_dimensions.y)&RUNTIME_CONTROLS_EVENT_CAPTURED)!=0,
        "global_key_captured","input");
    expect(s.controls.debug_overlay_enabled!=original_debug,"global_key_toggles_debug","input");
    expect(status->GetInnerRML()!=before,"global_key_synchronizes_menu","input");
    std::fprintf(stderr,"rml_ui_input_domain events=8192 motion_ms=%.3f os_events_injected=0\n",double(elapsed)/1e6);
  }
  s.controls.debug_overlay_enabled=original_debug;
  s.modal_was_open=original_modal;s.lighting_was_visible=original_lighting_was;
  s.release_input_pending=original_release;s.previous_screen=original_screen;
  const std::array sizes={Rml::Vector2i{640,480},Rml::Vector2i{1280,720},Rml::Vector2i{960,640},Rml::Vector2i{1920,1080}};
  auto within_viewport=[&](const char* id,Rml::Vector2i dimensions,bool focusable=false) {
    auto* control=s.document->GetElementById(id);
    if (!control) return;
    if(focusable) {
      // Scroll position persists across tested sizes; measure each focusable
      // where ScrollIntoView would place it for the user, scrolling every
      // nested panel as needed.
      control->ScrollIntoView({Rml::ScrollAlignment::Nearest,Rml::ScrollAlignment::Nearest,
          Rml::ScrollBehavior::Instant,Rml::ScrollParentage::All});
      s.context->Update();
    }
    const auto offset=control->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size=control->GetBox().GetSize(Rml::BoxArea::Border);
    const bool finite=std::isfinite(offset.x) && std::isfinite(offset.y) &&
        std::isfinite(size.x) && std::isfinite(size.y);
    const bool visible=control->IsVisible(true) && size.x>0 && size.y>0;
    const bool fits=finite && offset.x>=-1 && offset.y>=-1 &&
        offset.x+size.x<=float(dimensions.x)+1.f && offset.y+size.y<=float(dimensions.y)+1.f;
    expect(visible,"visible_layout",id);
    expect(fits,"viewport_bounds",id);
    if (!fits) std::fprintf(stderr,"rml_ui_bounds element=%s viewport=%dx%d box=%.1f,%.1f,%.1f,%.1f\n",
        id,dimensions.x,dimensions.y,offset.x,offset.y,size.x,size.y);
    if (focusable) {
      const auto& style=control->GetComputedValues();
      expect(style.focus()==Rml::Style::Focus::Auto && style.tab_index()==Rml::Style::TabIndex::Auto,
             "keyboard_focus_eligible",id);
    }
  };
  for (const auto dimensions:sizes) {
    s.context->SetDimensions(dimensions);
    s.context->SetDensityIndependentPixelRatio(1.f);
    s.document->SetClass("compact",dimensions.y<700);
    s.lighting.visible=false;
    s.controls.display_menu.active=1;
    for (unsigned screen=0;screen<=DISPLAY_MENU_SCREEN_INGAME;++screen) {
      s.controls.display_menu.screen=screen;
      s.sync_menu();s.sync_lighting();s.context->Update();
      within_viewport("menu",dimensions);
      if (screen==DISPLAY_MENU_SCREEN_SETTINGS) {
        for (const auto* id:setting_ids) within_viewport(id,dimensions,true);
        within_viewport("apply",dimensions,true);
      } else if (screen==DISPLAY_MENU_SCREEN_SINGLEPLAYER) {
        within_viewport("world-name",dimensions,true);
      } else if (screen==DISPLAY_MENU_SCREEN_MULTIPLAYER) {
        within_viewport("server-address",dimensions,true);
        within_viewport("server-port",dimensions,true);
      }
    }
    s.controls.display_menu.active=0;
    s.sync_menu();s.context->Update();
    within_viewport("crosshair",dimensions);
    if(s.inventory.selected_block()!=0) within_viewport("selected-block",dimensions);
    else if(auto* icon=s.document->GetElementById("selected-block"))
      expect(!icon->IsVisible(true),"empty_hand_has_no_block_icon","selected-block");
    s.lighting.visible=true;
    s.sync_menu();s.context->Update();
    within_viewport("lighting",dimensions);
    for (const auto* id:light_ids) {
      within_viewport(id,dimensions,true);
      within_viewport((std::string(id)+"-number").c_str(),dimensions,true);
    }
    within_viewport("close-lighting",dimensions,true);
  }
  s.controls.display_menu=original_menu;
  s.lighting.visible=original_lighting;
  s.context->SetDimensions(original_dimensions);
  s.context->SetDensityIndependentPixelRatio(original_density);
  s.document->SetClass("compact",original_compact);
  s.sync_menu();s.sync_lighting();s.context->Update();
  expect(s.system.errors==0 && s.system.warnings==0,"rmlui_diagnostics","document");
  std::fprintf(stderr,"rml_ui_contract=%s checks=%u failures=%u viewports=4 settings=%zu sliders=4\n",
      failures?"failed":"passed",checks,failures,setting_ids.size());
  return failures==0;
}
}
