#include "GameUiState.h"
#include <cmath>
#include <limits>
#include <tuple>

namespace octaryn::client::app {
bool GameUi::validate_loading_contract()
{
  auto& s=*state_;
  const auto original=std::make_tuple(s.loading_visible,s.loading_cancelable,s.loading_cancel_requested,
      s.loading_cancelling,s.loading_fraction,s.loading_started,s.loading_title,s.loading_status,s.loading_detail,
      s.lighting.visible,s.inventory_open,s.controls_open,s.fsr_open,s.controls.display_menu,s.controls.session_active);
  const auto dimensions=s.context->GetDimensions();
  const auto density=s.context->GetDensityIndependentPixelRatio();
  const auto original_pointer=s.loading_pointer;
  unsigned checks{},failures{};
  auto expect=[&](bool valid,const char* requirement) {
    ++checks;if(valid)return;
    ++failures;std::fprintf(stderr,"loading_ui_check=failed requirement=%s\n",requirement);
  };
  auto element=[&](const char* id){return s.document->GetElementById(id);};
  auto visible=[&](const char* id){auto* e=element(id);return e && e->IsVisible(true);};
  auto content=[&](const char* id){auto* e=element(id);return e?e->GetInnerRML():Rml::String{};};
  s.controls.session_active=0;s.loading_visible=false;
  s.lighting.visible=s.inventory_open=s.controls_open=s.fsr_open=false;
  show_world_library();s.context->Update();
  expect(s.library.document && s.library.document->IsVisible(),"library_visible_before_loading");
  show_loading("Fixture world");s.context->Update();
  expect(!s.library.document->IsVisible() && visible("loading-veil") && visible("loading-screen"),
      "loading_covers_separate_world_library_document");
  expect(!visible("loading-progress") && element("loading-bar-fill")->IsClassSet("indeterminate"),
      "unknown_progress_has_animation_without_percentage");
  auto* cancel=element("loading-cancel");
  if(cancel)cancel->DispatchEvent("click",{});
  expect(!take_loading_cancel() && !visible("loading-cancel"),"unsafe_phase_has_no_cancel_action");
  update_loading("Reading world","Fixture world / Save 2",0);s.context->Update();
  expect(content("loading-progress")=="0%" && !element("loading-bar-fill")->IsClassSet("indeterminate"),
      "measured_zero_progress_stays_zero");
  expect(content("loading-detail")=="Fixture world / Save 2","selected_world_and_save_are_visible");
  update_loading("Uploading geometry","3 of 12 pages",.25f);s.context->Update();
  expect(content("loading-progress")=="25%","measured_progress_is_displayed");
  const auto* width=element("loading-bar-fill")->GetProperty(Rml::PropertyId::Width);
  expect(width && std::abs(width->Get<float>()-25.f)<.01f,"measured_progress_drives_bar_width");
  for(float fraction:{-1.f,std::numeric_limits<float>::quiet_NaN()}) {
    update_loading("Compiling shaders","",fraction);s.context->Update();
    expect(!visible("loading-progress") && !visible("loading-detail"),"unknown_stage_hides_stale_percentage_and_detail");
  }
  s.loading_started=SDL_GetTicks();s.sync_loading();
  expect(content("loading-elapsed")=="Elapsed 0:00","elapsed_time_starts_from_actual_clock");
  set_loading_cancelable(true);s.context->Update();
  expect(cancel && visible("loading-cancel") && !cancel->HasAttribute("disabled"),"safe_phase_exposes_cancel");
  if(cancel)cancel->DispatchEvent("click",{});
  expect(take_loading_cancel() && !take_loading_cancel(),"cancellation_is_delivered_once");
  update_loading("Reading world","Waiting for worker checkpoint",.75f);
  set_loading_cancelable(true);s.context->Update();
  expect(content("loading-status")=="Cancelling..." && cancel->HasAttribute("disabled") && !visible("loading-progress"),
      "cancel_acknowledgement_survives_worker_progress_until_completion");
  if(cancel)cancel->DispatchEvent("click",{});
  expect(!take_loading_cancel(),"repeated_cancel_does_not_queue_more_work");
  show_loading("Fixture world");set_loading_cancelable(true);
  update_loading("Preparing geometry","Fixture world / Save 2",-1);
  for(const auto size:{Rml::Vector2i{640,480},Rml::Vector2i{1280,720},Rml::Vector2i{1920,1080}}) {
    s.context->SetDimensions(size);s.context->SetDensityIndependentPixelRatio(1);s.context->Update();
    for(const char* id:{"loading-title","loading-status","loading-detail","loading-elapsed","loading-cancel"}) {
      auto* e=element(id);expect(e!=nullptr,"required_loading_control");if(!e)continue;
      const auto offset=e->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto extent=e->GetBox().GetSize(Rml::BoxArea::Border);
      expect(e->IsVisible(true) && extent.x>0 && extent.y>0 && offset.x>=0 && offset.y>=0 &&
          offset.x+extent.x<=static_cast<float>(size.x)+1.f && offset.y+extent.y<=static_cast<float>(size.y)+1.f,
          "loading_control_fits_viewport");
    }
  }
  auto pure_phase=[&] {
    show_loading("Exclusive renderer work");set_loading_cancelable(true);
    s.context->Update();s.cache_loading_input();
  };
  pure_phase();
  const auto pixel_size=s.context->GetDimensions();
  const auto before=s.document->GetInnerRML();
  SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.key=SDLK_ESCAPE;
  key.key.windowID=s.loading_pointer.window_id;key.key.repeat=true;
  loading_event(key,pixel_size.x,pixel_size.y);
  expect(!take_loading_cancel(),"exclusive_loading_repeated_escape_is_ignored");
  key.key.repeat=false;key.key.windowID=s.loading_pointer.window_id+1;
  loading_event(key,pixel_size.x,pixel_size.y);
  expect(!take_loading_cancel(),"exclusive_loading_foreign_window_input_is_ignored");
  key.key.windowID=s.loading_pointer.window_id;
  expect(loading_event(key,pixel_size.x,pixel_size.y) && take_loading_cancel() && !take_loading_cancel(),
      "exclusive_loading_escape_delivers_cancel_once");
  expect(s.document->GetInnerRML()==before && s.context->GetDimensions()==pixel_size,
      "exclusive_loading_cancel_does_not_touch_rml_tree_or_layout");
  pure_phase();
  const auto box=s.loading_pointer;
  const float x=(box.left+box.right)/(2*box.density),y=(box.top+box.bottom)/(2*box.density);
  auto pointer=[&](std::uint32_t type,float px,float py,std::uint8_t button=SDL_BUTTON_LEFT,int width=0) {
    SDL_Event input{};input.type=type;input.button.windowID=box.window_id;
    input.button.button=button;input.button.x=px;input.button.y=py;
    return loading_event(input,width?width:pixel_size.x,pixel_size.y);
  };
  pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y);
  expect(!take_loading_cancel(),"exclusive_loading_release_requires_matching_press");
  pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,-1,-1);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y);
  expect(!take_loading_cancel(),"exclusive_loading_outside_press_cannot_activate_cancel");
  pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);pointer(SDL_EVENT_MOUSE_BUTTON_UP,-1,-1);
  expect(!take_loading_cancel(),"exclusive_loading_drag_release_outside_does_not_cancel");
  pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y,SDL_BUTTON_RIGHT);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y,SDL_BUTTON_RIGHT);
  expect(!take_loading_cancel(),"exclusive_loading_only_left_click_activates_cancel");
  pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);
  SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;
  loading_event(focus,pixel_size.x,pixel_size.y);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y);
  expect(!take_loading_cancel(),"exclusive_loading_focus_loss_clears_stale_press");
  pure_phase();pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);
  pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y,SDL_BUTTON_LEFT,pixel_size.x+1);
  expect(!take_loading_cancel(),"exclusive_loading_resize_rejects_stale_button_bounds");
  pure_phase();
  const auto pointer_before=s.document->GetInnerRML();
  pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x,y);
  expect(take_loading_cancel() && s.document->GetInnerRML()==pointer_before,
      "exclusive_loading_valid_click_cancels_without_rml_mutation");
  set_loading_cancelable(false);key.key.repeat=false;
  loading_event(key,pixel_size.x,pixel_size.y);
  expect(!take_loading_cancel(),"exclusive_loading_disabled_cancel_ignores_escape");
  hide_loading();s.context->Update();
  expect(s.library.document->IsVisible() && !visible("loading-veil"),"completion_restores_library_surface");
  expect(!take_loading_cancel(),"completion_clears_pending_cancel");
  const auto selected=s.library.selected_id,save=s.library.selected_save,query=s.library.query;
  show_loading("Cancelled world");set_loading_cancelable(true);s.context->Update();
  if(cancel)cancel->DispatchEvent("click",{});
  expect(s.loading_cancel_requested && s.loading_cancelling,"return_fixture_uses_real_cancel_handler");
  show_world_library();s.context->Update();
  expect(world_library_visible() && !loading_visible() && !visible("loading-screen") && !visible("loading-veil"),
      "show_world_library_replaces_cancelled_loading_surface");
  expect(!take_loading_cancel() && !s.loading_cancelling && !s.loading_cancelable && !s.loading_pointer.valid,
      "show_world_library_clears_loading_cancel_state");
  expect(s.library.selected_id==selected && s.library.selected_save==save && s.library.query==query,
      "return_to_library_preserves_selected_world_save_and_search");
  std::tie(s.loading_visible,s.loading_cancelable,s.loading_cancel_requested,s.loading_cancelling,
      s.loading_fraction,s.loading_started,s.loading_title,s.loading_status,s.loading_detail,
      s.lighting.visible,s.inventory_open,s.controls_open,s.fsr_open,s.controls.display_menu,s.controls.session_active)=original;
  s.loading_pointer=original_pointer;
  s.context->SetDimensions(dimensions);s.context->SetDensityIndependentPixelRatio(density);
  s.sync_menu();s.sync_capture();s.context->Update();
  expect(s.system.errors==0 && s.system.warnings==0,"rmlui_diagnostics_clean");
  std::fprintf(stderr,"loading_ui_contract=%s checks=%u failures=%u injected_os_events=0\n",
      failures?"failed":"passed",checks,failures);
  return failures==0;
}
}
