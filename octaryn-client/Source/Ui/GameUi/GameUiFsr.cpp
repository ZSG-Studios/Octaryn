#include "GameUiState.h"
#include "Menu.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
namespace {
std::string number(float value) {
  char result[32];std::snprintf(result,sizeof(result),"%.1f",value);return result;
}
void disabled(Rml::ElementDocument* doc,const char* id,bool value) {
  if(auto* e=doc->GetElementById(id)) {
    if(value)e->SetAttribute("disabled",true);else e->RemoveAttribute("disabled");
  }
}
}
void GameUi::set_render_resolution(unsigned w,unsigned h,unsigned dw,unsigned dh) {
  state_->fsr_width=w;state_->fsr_height=h;state_->fsr_display_width=dw;state_->fsr_display_height=dh;
}
void GameUi::show_fsr_settings() {
  auto& s=*state_;runtime_controls_copy_to_menu(&s.controls,s.window);
  s.controls.display_menu.active=1;s.controls.display_menu.screen=DISPLAY_MENU_SCREEN_SETTINGS;
  s.inventory_open=false;s.controls_open=false;s.lighting.visible=false;s.fsr_open=true;
  s.sync_menu();s.sync_capture();
}
void GameUi::State::sync_fsr() {
  if(fsr_syncing)return;
  fsr_syncing=true;
  const auto& m=controls.display_menu;
  const bool on=m.upscaler_mode!=0, dynamic=on&&m.upscaler_mode!=1&&m.fsr_dynamic_resolution;
  constexpr const char* names[]={"Off","Native AA","Quality","Balanced","Performance","Ultra Performance","Custom"};
  constexpr const char* descriptions[]={
    "Native rendering without temporal reconstruction.",
    "Temporal anti-aliasing at full resolution. Best detail; no upscaling.",
    "Renders at about 67% per dimension. Favors image quality.",
    "Renders at about 59% per dimension. Balances detail and GPU cost.",
    "Renders at 50% per dimension. Trades fine detail for GPU headroom.",
    "Renders at about 33% per dimension. Best suited to high output resolutions.",
    "Choose the internal resolution as a percentage of each output dimension."};
  text("fsr-mode-value",names[std::min<unsigned>(m.upscaler_mode,6)]);
  text("fsr-description",dynamic?"Resolution adapts to GPU load within your limits. The FPS target is not a frame cap.":descriptions[std::min<unsigned>(m.upscaler_mode,6)]);
  text("fsr-sharpen-value",!on?"Inactive":m.fsr_sharpening?"On":"Off");
  text("fsr-dynamic-value",!on||m.upscaler_mode==1?"Inactive":m.fsr_dynamic_resolution?"On":"Off");
  document->GetElementById("fsr-min")->SetAttribute("max",number(m.fsr_max_scale*100));
  document->GetElementById("fsr-max")->SetAttribute("min",number(m.fsr_min_scale*100));
  const char* ids[]={"fsr-sharpness","fsr-scale","fsr-min","fsr-max","fsr-target"};
  const float values[]={m.fsr_sharpness*100,m.fsr_render_scale*100,m.fsr_min_scale*100,m.fsr_max_scale*100,float(m.fsr_target_fps)};
  for(unsigned i=0;i<5;++i) {
    text((std::string(ids[i])+"-value").c_str(),(i==4?std::to_string(m.fsr_target_fps)+" FPS":number(values[i])+"%"));
    if(auto* e=document->GetElementById(ids[i]);e &&
        e->GetAttribute<Rml::String>("value","")!=number(values[i]))e->SetAttribute("value",number(values[i]));
  }
  disabled(document,"fsr-sharpen",!on);disabled(document,"fsr-sharpness",!on||!m.fsr_sharpening);
  disabled(document,"fsr-scale",!on||m.upscaler_mode!=6||dynamic);
  disabled(document,"fsr-dynamic",!on||m.upscaler_mode==1);
  for(const char* id:{"fsr-min","fsr-max","fsr-target"})disabled(document,id,!dynamic);
  if(fsr_display_width&&fsr_display_height) {
    text("fsr-resolution","Current: "+std::to_string(fsr_width)+" x "+std::to_string(fsr_height)+
      " -> "+std::to_string(fsr_display_width)+" x "+std::to_string(fsr_display_height));
  } else text("fsr-resolution","Resolution appears when a world is running.");
  fsr_syncing=false;
}
bool GameUi::State::fsr_event(Rml::Event& e,Rml::Element* target) {
  auto& m=controls.display_menu;
  if(e.GetType()=="change" && target->HasAttribute("fsr-field")) {
    if(fsr_syncing)return true;
    if(target->HasAttribute("disabled"))return true;
    float value{};const auto input=e.GetParameter<Rml::String>("value","");
    if(std::sscanf(input.c_str(),"%f",&value)!=1||!std::isfinite(value))return true;
    const auto field=target->GetAttribute<Rml::String>("fsr-field","");
    if(field=="sharpness")m.fsr_sharpness=std::clamp(value/100,0.f,1.f);
    if(field=="scale")m.fsr_render_scale=std::clamp(value/100,1.f/3.f,1.f);
    if(field=="min")m.fsr_min_scale=std::clamp(value/100,1.f/3.f,m.fsr_max_scale);
    if(field=="max")m.fsr_max_scale=std::clamp(value/100,m.fsr_min_scale,1.f);
    if(field=="target")m.fsr_target_fps=static_cast<uint16_t>(std::clamp(std::round(value),30.f,240.f));
    sync_menu();return true;
  }
  if(e.GetType()!="click")return false;
  while(target&&!target->HasAttribute("action"))target=target->GetParentNode();
  if(!target)return false;
  const auto action=target->GetAttribute<Rml::String>("action","");
  if(action!="open-fsr"&&action.find("fsr-")!=0)return false;
  if(target->HasAttribute("disabled"))return true;
  if(action=="open-fsr")fsr_open=true;
  else if(action=="fsr-back")fsr_open=false;
  else if(action=="fsr-mode")m.upscaler_mode=(m.upscaler_mode+1)%7;
  else if(action=="fsr-sharpen")m.fsr_sharpening=!m.fsr_sharpening;
  else if(action=="fsr-dynamic")m.fsr_dynamic_resolution=!m.fsr_dynamic_resolution;
  else if(action=="fsr-reset") {
    m.upscaler_mode=0;m.fsr_sharpening=1;m.fsr_sharpness=.2f;m.fsr_render_scale=.667f;
    m.fsr_dynamic_resolution=0;m.fsr_min_scale=.5f;m.fsr_max_scale=1.f;m.fsr_target_fps=60;
  } else if(action=="fsr-apply") {
    pending|=runtime_controls_request_apply(&controls,window);fsr_open=false;
  }
  sync_menu();sync_capture();return true;
}
}
