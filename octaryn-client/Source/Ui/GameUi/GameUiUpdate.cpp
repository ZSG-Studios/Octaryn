#include "GameUiState.h"
#include "UiData.h"
#include "RenderDistance.h"
#include <algorithm>
#include <cstdio>

namespace octaryn::client::app {
namespace {
std::string decimal(unsigned value,unsigned scale=100) {
  if (value==UINT32_MAX) return "N/A";
  char text[48];std::snprintf(text,sizeof(text),"%.2f",double(value)/scale);return text;
}
void input_value(Rml::ElementDocument* document,const char* id,const std::string& value) {
  auto* element=document->GetElementById(id);
  if (element && !element->IsPseudoClassSet("focus") && element->GetAttribute<Rml::String>("value","")!=value)
    element->SetAttribute("value",value);
}
}
void GameUi::State::sync_menu() {
  const auto& menu=controls.display_menu;
  if(!menu.active || menu.screen!=DISPLAY_MENU_SCREEN_SETTINGS)fsr_open=false;
  visible("fsr-screen",fsr_open);
  document->GetElementById("menu")->SetClass("fsr-options",fsr_open);
  visible("menu",menu.active!=0 && !lighting.visible && !inventory_open);
  visible("lighting",lighting.visible);
  visible("hud",!modal_open());
  visible("scrim",modal_open() && !inventory_open);
  visible("inventory",inventory_open);
  visible("inventory-page",!creative_open);
  visible("creative-page",creative_open);
  visible("controls-screen",controls_open);
  const char* screens[]={"main-screen","worlds-screen","servers-screen","settings-screen","pause-screen"};
  for (unsigned i=0;i<5;++i) visible(screens[i],i==menu.screen && !controls_open && !fsr_open);
  text("display-value","Display "+std::to_string(std::max(0,menu.display_index)+1));
  std::string resolution="Unavailable";
  if (menu.mode_index>=0 && menu.mode_index<controls.display_catalog.mode_count) {
    const auto& mode=controls.display_catalog.modes[menu.mode_index];
    resolution=std::to_string(mode.pixel_width)+" x "+std::to_string(mode.pixel_height);
  }
  text("resolution-value",resolution);
  text("fullscreen-value",menu.fullscreen?"Fullscreen":"Windowed");
  constexpr const char* upscalers[]={"Off","Native AA","Quality","Balanced","Performance","Ultra performance","Custom"};
  text("upscaler-value",upscalers[std::min<unsigned>(menu.upscaler_mode,6)]);
  text("ray-tracing-value",controls.ray_tracing_available?(menu.ray_tracing_enabled?"On":"Off"):"Unavailable");
  if(auto* ray=document->GetElementById("ray-tracing")) {
    ray->SetClass("enabled",controls.ray_tracing_available && menu.ray_tracing_enabled);
    if(controls.ray_tracing_available)ray->RemoveAttribute("disabled");
    else ray->SetAttribute("disabled","");
  }
  text("distance-value",std::to_string(render_distance_options()[std::clamp(menu.render_distance_index,
      0,render_distance_option_count()-1)])+" chunks");
  const unsigned flags[]={menu.fog_enabled,menu.clouds_enabled,menu.sky_gradient_enabled,menu.stars_enabled,
                         menu.sun_enabled,menu.moon_enabled,menu.pom_enabled,menu.pbr_enabled};
  const char* ids[]={"fog","clouds","sky","stars","sun","moon","pom","pbr"};
  for (int i=0;i<8;++i) {
    text((std::string(ids[i])+"-value").c_str(),flags[i]?"On":"Off");
    if (auto* element=document->GetElementById(ids[i])) element->SetClass("enabled",flags[i]!=0);
  }
  const char* statuses[]={"Choose your next adventure.","World selected.","Editing name.",
    "Delete this world? Confirm below to permanently remove it.","World deleted.","That world is missing.",
    "World loaded.","World created.","World saved.","Enter a valid server address and port.",
    "Connected.","This action is unavailable in the current session.","This world is already active.",
    "That world already exists.","The action could not be completed."};
  text("menu-status",statuses[std::min(menu.status_code,14u)]);
  visible("delete-confirm",menu.status_code==DISPLAY_MENU_STATUS_DELETE_CONFIRM);
  for (unsigned i=0;i<3;++i) {
    const std::string id="world-"+std::to_string(i);
    if (auto* element=document->GetElementById(id)) {
      const bool exists=(menu.world_exists_mask&(1u<<i))!=0;
      element->SetClass("selected",menu.world_slot==i);
      if (exists && element->HasAttribute("disabled")) element->RemoveAttribute("disabled");
      else if (!exists && !element->HasAttribute("disabled")) element->SetAttribute("disabled",true);
      text((id+"-state").c_str(),exists?"Ready to explore":"Empty slot");
    }
  }
  if(fsr_open)sync_fsr();
  input_value(document,"world-name",menu.world_name);
  input_value(document,"server-address",menu.server_address);
  input_value(document,"server-port",menu.server_port);
}
void GameUi::State::sync_lighting() {
  const float values[]={lighting.values.ambient_strength,lighting.values.sun_strength,
                        lighting.values.fog_distance,lighting.values.skylight_floor};
  const char* ids[]={"ambient","sun-strength","fog-distance","sky-floor"};
  for (int i=0;i<4;++i) {
    char number[32];std::snprintf(number,sizeof(number),"%.2f",values[i]);
    text((std::string(ids[i])+"-value").c_str(),number);
    input_value(document,ids[i],number);
    input_value(document,(std::string(ids[i])+"-number").c_str(),number);
  }
  text("fallback-value",decimal(static_cast<unsigned>(lighting.values.sun_fallback_strength*100)));
}
void GameUi::update(const rendering::UiDrawData& p,unsigned atlas_tile,int width,int height) {
  auto& s=*state_;
  s.update_profile.begin();
  s.context->SetDimensions({std::max(1,width),std::max(1,height)});
  // Integer density keeps pixel borders aligned; small windows use compact layout.
  s.context->SetDensityIndependentPixelRatio(height>=1440?2.f:1.f);
  s.document->SetClass("compact",height<700);
  s.update_profile.mark(0);
  if (s.modal_open() || s.modal_was_open) s.sync_menu();
  s.sync_inventory();
  s.update_profile.mark(1);
  if (s.lighting.visible) s.sync_lighting();
  s.update_profile.mark(2);
  s.sync_capture();s.update_profile.mark(3);
  s.visible("diagnostics",p.DebugEnabled!=0);
  if (s.last_tile!=atlas_tile) {
    if (auto* image=s.document->GetElementById("selected-block"))
      image->SetAttribute("rect",std::to_string(atlas_tile*32)+" 0 32 32");
    s.last_tile=atlas_tile;
  }
  const double now=s.system.GetElapsedTime();
  s.visible("inventory-toast",now<s.inventory_toast_until);
  const bool refreshed=p.DebugEnabled && now>=s.metrics_at+.25;
  if (refreshed) {
  s.metrics_at=now;
  s.text("fps",decimal(p.FPSTenths,10));
  s.metrics.update(s.document->GetElementById("metrics"),{
      p.FrameTimeHundredths,p.ProfileFrameTimeHundredths,p.FPSAverageTenths,p.FPSLow1Tenths,
      p.FPSLow01Tenths,p.FPSLowX5Tenths,p.FPSLowX10Tenths,p.FPSWorstTenths,
      p.MSLow1Hundredths,p.MSLow01Hundredths,p.MSLowX5Hundredths,p.MSLowX10Hundredths,
      p.MSWorstHundredths,p.CpuLoadHundredths,p.GpuLoadHundredths,p.CpuRamHundredthsGiB,
      p.GpuVramHundredthsGiB,p.WorldTimeHundredths,p.RenderTimeHundredths});
  s.text("samples",std::to_string(p.SampleCount)+" samples / "+
      (p.WarmupComplete?"ready":decimal(p.WarmupElapsedHundredths)+" / "+decimal(p.WarmupTotalHundredths)+"s warmup"));
  }
  s.update_profile.mark(4);
  s.context->Update();
  s.update_profile.mark(5);
  s.release_input();
  s.update_profile.mark(6);
  s.update_profile.finish(refreshed);
}
}
