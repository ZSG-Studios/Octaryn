#include "GameUiState.h"
#include "Menu.h"
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
bool GameUi::validate_fsr_contract() {
  auto& s=*state_;const auto original=s.controls;const bool prior=s.fsr_open;
  const bool lighting=s.lighting.visible,inventory=s.inventory_open,controls=s.controls_open;
  const auto pending=s.pending;const auto dimensions=s.context->GetDimensions();
  const auto density=s.context->GetDensityIndependentPixelRatio();
  const bool compact=s.document->IsClassSet("compact");
  unsigned checks{},failures{};
  auto expect=[&](bool value,const char* message){++checks;if(!value){++failures;std::fprintf(stderr,"fsr_ui_check=failed %s\n",message);}};
  auto click=[&](const char* selector) {
    auto* e=s.document->QuerySelector(selector);expect(e!=nullptr,selector);
    if(e)e->DispatchEvent("click",{});s.sync_menu();s.context->Update();
  };
  auto change=[&](const char* id,const char* value) {
    auto* e=s.document->GetElementById(id);expect(e!=nullptr,id);
    if(e){Rml::Dictionary args;args["value"]=Rml::String(value);e->DispatchEvent("change",args);}
    s.sync_menu();s.context->Update();
  };
  s.lighting.visible=false;s.inventory_open=false;s.controls_open=false;
  s.controls.display_menu.active=1;s.controls.display_menu.screen=DISPLAY_MENU_SCREEN_SETTINGS;
  s.controls.display_menu.upscaler_mode=2;s.fsr_open=false;
  const auto before_hover=s.controls.display_menu;
  for(const char* selector:{"#distance","#fullscreen","#apply","[action=close-lighting]"}) {
    auto* control=s.document->QuerySelector(selector);expect(control!=nullptr,selector);
    if(control)for(const char* type:{"mousemove","mouseover","drag","dragend"})control->DispatchEvent(type,{});
    expect(s.controls.display_menu.render_distance_index==before_hover.render_distance_index &&
      s.controls.display_menu.fullscreen==before_hover.fullscreen &&
      s.controls.display_menu.active==before_hover.active &&
      s.controls.display_menu.screen==before_hover.screen,"hover_and_drag_do_not_activate_menu");
  }
  click("[action=open-fsr]");expect(s.fsr_open,"open_fsr_page");
  auto& m=s.controls.display_menu;
  const auto active_mode=s.controls.upscaler_mode;
  m.fsr_sharpening=1;s.sync_menu();
  change("fsr-sharpness","73");expect(std::abs(m.fsr_sharpness-.73f)<.001f,"sharpness_percent");
  expect(s.controls.upscaler_mode==active_mode,"staged_mode_does_not_apply_early");
  m.upscaler_mode=6;m.fsr_dynamic_resolution=0;s.sync_menu();
  change("fsr-scale","72.5");expect(std::abs(m.fsr_render_scale-.725f)<.001f,"custom_scale_percent");
  click("#fsr-dynamic");expect(m.fsr_dynamic_resolution==1,"dynamic_toggle");
  change("fsr-min","55");change("fsr-max","85");change("fsr-target","120");
  expect(std::abs(m.fsr_min_scale-.55f)<.001f&&std::abs(m.fsr_max_scale-.85f)<.001f&&m.fsr_target_fps==120,"dynamic_limits");
  change("fsr-min","99");expect(m.fsr_min_scale==m.fsr_max_scale,"minimum_cannot_exceed_maximum");
  change("fsr-max","40");expect(m.fsr_min_scale==m.fsr_max_scale,"maximum_cannot_cross_minimum");
  change("fsr-target","nan");expect(m.fsr_target_fps==120,"nonfinite_input_ignored");
  m.upscaler_mode=1;s.sync_menu();
  expect(s.document->GetElementById("fsr-dynamic")->HasAttribute("disabled"),"native_disables_dynamic");
  click("#fsr-dynamic");expect(m.fsr_dynamic_resolution==1,"disabled_toggle_preserves_preference");
  m.upscaler_mode=0;s.sync_menu();
  expect(s.document->GetElementById("fsr-sharpness")->HasAttribute("disabled"),"off_disables_sharpening");
  change("fsr-sharpness","10");expect(std::abs(m.fsr_sharpness-.73f)<.001f,"disabled_slider_ignored");
  m.upscaler_mode=6;
  click("[action=fsr-apply]");
  expect(s.controls.upscaler_mode==6&&s.controls.fsr_target_fps==120&&std::abs(s.controls.fsr_sharpness-.73f)<.001f,"apply_reaches_runtime_controls");
  s.lighting.visible=false;s.inventory_open=false;s.controls_open=false;
  s.controls.display_menu.active=1;s.controls.display_menu.screen=DISPLAY_MENU_SCREEN_SETTINGS;s.fsr_open=true;
  click("[action=fsr-reset]");expect(m.upscaler_mode==0&&m.fsr_sharpening&&m.fsr_sharpness==.2f&&m.fsr_target_fps==60&&!m.fsr_dynamic_resolution,"reset_defaults_staged");
  expect(s.controls.upscaler_mode==6,"reset_does_not_apply_early");
  for(const auto size:{Rml::Vector2i{640,480},Rml::Vector2i{1280,720},Rml::Vector2i{2560,1440}}) {
    s.context->SetDimensions(size);s.context->SetDensityIndependentPixelRatio(size.y>=1440?2.f:1.f);
    s.document->SetClass("compact",size.y<700);s.fsr_open=true;s.sync_menu();s.context->Update();
    for(const char* id:{"fsr-mode","fsr-sharpness","fsr-scale","fsr-dynamic","fsr-min","fsr-max","fsr-target"}) {
      auto* e=s.document->GetElementById(id);const auto pos=e->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto extent=e->GetBox().GetSize(Rml::BoxArea::Border);
      expect(extent.x>0&&extent.y>0&&pos.x>=0&&pos.x+extent.x<=float(size.x)+1.f,"fsr_controls_horizontal_bounds");
    }
  }
  s.controls=original;s.fsr_open=prior;s.pending=pending;
  s.lighting.visible=lighting;s.inventory_open=inventory;s.controls_open=controls;
  s.context->SetDimensions(dimensions);s.context->SetDensityIndependentPixelRatio(density);
  s.document->SetClass("compact",compact);s.sync_menu();s.sync_capture();s.context->Update();
  std::printf("fsr_ui_contract=%s checks=%u failures=%u\n",failures?"failed":"passed",checks,failures);
  return failures==0;
}
}
