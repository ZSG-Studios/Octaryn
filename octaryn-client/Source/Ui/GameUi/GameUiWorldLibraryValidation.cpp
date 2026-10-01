#include "GameUiState.h"
#include <array>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
bool GameUi::validate_world_library_contract() {
  auto& s=*state_;
  auto original=s.library;
  const auto original_menu=s.controls.display_menu;
  const auto original_dimensions=s.context->GetDimensions();
  const auto original_density=s.context->GetDensityIndependentPixelRatio();
  const bool original_session=s.controls.session_active,original_lighting=s.lighting.visible;
  const bool original_loading=s.loading_visible,original_inventory=s.inventory_open;
  const bool original_controls=s.controls_open,original_fsr=s.fsr_open;
  unsigned checks{},failures{};
  auto expect=[&](bool valid,const char* requirement) {
    ++checks;
    if(!valid){++failures;std::fprintf(stderr,"world_library_ui_check=failed requirement=%s\n",requirement);}
  };
  if(!s.library.document || !s.library.model)return false;
  s.controls.session_active=0;s.lighting.visible=false;s.loading_visible=false;
  s.inventory_open=false;s.controls_open=false;s.fsr_open=false;
  s.library.query.clear();s.library.selected_id.clear();s.library.actions.clear();
  std::vector<WorldLibraryEntry> entries;
  for(unsigned index=0;index<12;++index) {
    WorldLibraryEntry entry;
    entry.id="fixture-"+std::to_string(index);entry.name="World "+std::to_string(index);
    entry.source="C:/Worlds/scene "+std::to_string(index)+".glb";entry.format="GLB";
    entry.save_label="Save 1";entry.last_played="Not played yet";entry.available=true;
    entries.push_back(std::move(entry));
  }
  entries[0].name="<img id='untrusted-world-name'/> & Bistro";
  entries[0].saves={{"first","Save 1","Today"},{"second","Save 2","Today"}};
  entries[0].active_save="second";
  entries[1].name=entries[2].name="Twin";entries[1].available=false;
  entries[3].available=false;entries[3].preparation_required=true;
  set_world_library(entries,"Import error <source> & detail",false,false,true);show_world_library();s.context->Update();
  expect(s.library.rows.size()==12,"unbounded_by_legacy_three_slots");
  expect(s.library.document->GetElementById("untrusted-world-name")==nullptr,"name_is_text_not_markup");
  auto* list=s.library.document->GetElementById("library-list");
  unsigned visible_rows{};
  if(list)for(int index=0;index<list->GetNumChildren();++index)
    if(list->GetChild(index)->IsVisible(true))++visible_rows;
  expect(visible_rows==12,"bound_list_renders_all_worlds");
  auto* status=s.library.document->GetElementById("library-status");
  expect(status && status->GetInnerRML().find("Import error")!=std::string::npos,"inline_error_visible");
  auto* feedback=s.library.document->GetElementById("library-feedback");
  expect(feedback && feedback->IsClassSet("failed"),"failure_has_explicit_visual_state");
  expect(s.library.multiple_saves,"older_saves_available");
  expect(s.library.selected_save=="second","active_save_selected_when_timestamps_tie");
  auto change=[&](const char* id,const std::string& value) {
    auto* element=s.library.document->GetElementById(id);
    expect(element!=nullptr,"bound_input_exists");if(!element)return;
    Rml::Dictionary parameters;parameters["value"]=Rml::String(value);
    element->DispatchEvent("change",parameters);s.context->Update();
  };
  change("library-save","second");
  if(auto* open=s.library.document->GetElementById("library-open"))open->DispatchEvent("click",{});
  WorldLibraryAction action;
  expect(take_world_library_action(action) && action.kind==WorldLibraryActionKind::Open &&
      action.world_id==entries[0].id && action.save_id=="second","chosen_save_open_action");
  s.context->Update();
  expect(s.library.busy && s.library.open_label=="Opening..." && !s.library.failed && s.library.has_status,
      "open_acknowledged_before_host_takes_work");
  s.library_action("open",{});s.library_action("select",{Rml::Variant(entries[4].id)});
  change("library-save","first");change("library-search","Twin");
  expect(!take_world_library_action(action) && s.library.selected_id==entries[0].id &&
      s.library.selected_save=="second" && s.library.query.empty(),"busy_action_freezes_selection_and_rejects_duplicates");
  set_world_library(entries,"",false);s.context->Update();
  s.library_action("new_save",{});s.context->Update();
  expect(take_world_library_action(action) && action.kind==WorldLibraryActionKind::NewSave &&
      action.world_id==entries[0].id && s.library.new_save_label=="Creating save..." && s.library.busy,
      "new_save_acknowledged_with_selected_world_identity");
  set_world_library(entries,"",false);s.context->Update();
  if(list && list->GetChild(4) && list->GetChild(4)->GetChild(0)) {
    list->GetChild(4)->GetChild(0)->Focus();
    expect(s.library.selected_id==entries[4].id,"keyboard_focus_selects_world");
    SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.key=SDLK_RETURN;
    expect(s.world_library_key(key) && take_world_library_action(action) && action.world_id==entries[4].id,
        "keyboard_return_opens_focused_world");
  } else expect(false,"keyboard_world_row_exists");
  set_world_library(entries,"",false);s.context->Update();
  s.library_action("select",{Rml::Variant(entries[1].id)});
  s.library_action("open",{});
  expect(!take_world_library_action(action),"missing_source_cannot_open");
  s.library_action("new_save",{});
  expect(!take_world_library_action(action),"missing_source_cannot_create_save");
  s.library_action("locate",{});
  expect(take_world_library_action(action) && action.kind==WorldLibraryActionKind::Locate &&
      action.world_id==entries[1].id,"locate_preserves_world_identity");
  expect(s.library.busy && s.library.status.find("Choose the source file")!=std::string::npos,"locate_acknowledged_immediately");
  set_world_library(entries,"",false);s.context->Update();
  s.library_action("select",{Rml::Variant(entries[3].id)});s.context->Update();
  expect(s.library.preparation_required && !s.library.missing && !s.library.can_open,
      "unprepared_source_has_distinct_state");
  expect(s.library.open_label=="Preparation needed","preparation_action_label");
  if(auto* open=s.library.document->GetElementById("library-open"))
    expect(open->HasAttribute("disabled") && !open->IsVisible(true),"unprepared_source_only_shows_relevant_action");
  else expect(false,"preparation_open_control_exists");
  for(const char* action_id:{"open","new_save","locate"})s.library_action(action_id,{});
  expect(!take_world_library_action(action),"unprepared_source_actions_rejected");
  auto* prepare=s.library.document->GetElementById("library-prepare");
  expect(prepare && prepare->IsVisible(true),"preparation_control_visible");
  if(prepare)prepare->DispatchEvent("click",{});
  expect(take_world_library_action(action) && action.kind==WorldLibraryActionKind::Prepare &&
      action.world_id==entries[3].id,"prepare_uses_selected_world_identity");
  expect(s.library.busy && s.library.prepare_label=="Preparing...","prepare_acknowledged_immediately");
  set_world_library(entries,"",false);s.context->Update();
  s.library_action("select",{Rml::Variant(entries[2].id)});
  expect(s.library.selected_id==entries[2].id,"duplicate_names_select_by_identity");
  change("library-search","Twin");
  expect(s.library.rows.size()==2,"search_filters_by_name");
  expect(s.library.name=="Twin","search_selection_matches_results");
  s.library.query="scene 11";s.rebuild_world_library();
  expect(s.library.rows.size()==1 && s.library.rows.front().id==entries[11].id,"search_filters_source");
  s.library.query="no such scene";s.rebuild_world_library();
  expect(s.library.no_matches && !s.library.empty,"search_empty_state");
  expect(!s.library.has_selection && !s.library.can_open,"no_matches_cannot_open_hidden_world");
  s.library.query.clear();s.library.selected_id=entries[0].id;s.rebuild_world_library();
  set_world_library(entries,"Adding worlds...",true);s.context->Update();
  for(const char* id:{"library-add","library-find","library-open","library-new-save","library-search"}) {
    auto* element=s.library.document->GetElementById(id);
    expect(element && element->HasAttribute("disabled"),"busy_controls_disabled");
  }
  for(const char* action_id:{"browse","find","open","new_save","locate"})s.library_action(action_id,{});
  expect(!take_world_library_action(action),"busy_actions_rejected");
  s.library_action("cancel",{});
  expect(!take_world_library_action(action),"file_dialog_does_not_offer_preparation_cancel");
  set_world_library(entries,"Preparing world: 2 / 5",true,true);s.context->Update();
  auto* cancel=s.library.document->GetElementById("library-cancel");
  expect(cancel && cancel->IsVisible(true),"preparation_cancel_visible");
  if(cancel)cancel->DispatchEvent("click",{});
  expect(take_world_library_action(action) && action.kind==WorldLibraryActionKind::Cancel,
      "busy_preparation_can_cancel");
  expect(!s.library.cancelable && s.library.busy && s.library.status=="Stopping preparation...",
      "cancel_acknowledged_until_worker_checkpoint");
  set_world_library(entries,"Stopping preparation...",true,false);s.context->Update();
  s.library_action("cancel",{});
  expect(!take_world_library_action(action),"cancel_waits_for_worker_checkpoint");
  set_world_library(entries,"",false);s.context->Update();
  expect(!s.library.has_status,"idle_library_has_no_filler_status_bar");
  for(const auto dimensions:{Rml::Vector2i{640,480},Rml::Vector2i{1280,720},Rml::Vector2i{1920,1080}}) {
    s.context->SetDimensions(dimensions);s.context->SetDensityIndependentPixelRatio(1.f);s.context->Update();
    for(const char* id:{"world-library","library-search","library-add","library-find","library-open","library-new-save","library-settings","library-multiplayer"}) {
      auto* element=s.library.document->GetElementById(id);
      expect(element!=nullptr,"required_control");if(!element)continue;
      element->ScrollIntoView({Rml::ScrollAlignment::Nearest,Rml::ScrollAlignment::Nearest,
          Rml::ScrollBehavior::Instant,Rml::ScrollParentage::All});s.context->Update();
      const auto offset=element->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto size=element->GetBox().GetSize(Rml::BoxArea::Border);
      const bool fits=element->IsVisible(true) && std::isfinite(offset.x) && std::isfinite(offset.y) &&
          size.x>0 && size.y>0 && offset.x>=-1 && offset.y>=-1 &&
          offset.x+size.x<=static_cast<float>(dimensions.x)+1.f && offset.y+size.y<=static_cast<float>(dimensions.y)+1.f;
      expect(fits,"control_within_viewport");
      if(std::string_view(id)=="library-open")expect(size.x>=120.f,"primary_action_has_readable_width");
      if(std::string_view(id)=="library-open" && size.x<120.f) {
        for(auto* node=element;node;node=node->GetParentNode()) {
          const auto border=node->GetBox().GetSize(Rml::BoxArea::Border);
          const auto content=node->GetBox().GetSize(Rml::BoxArea::Content);
          const auto* width=node->GetProperty("width");
          std::fprintf(stderr,"world_library_layout element=%s class=%s border=%.1f,%.1f content=%.1f,%.1f width=%s\n",
              node->GetId().c_str(),node->GetClassNames().c_str(),border.x,border.y,content.x,content.y,width?width->ToString().c_str():"missing");
        }
      }
      if(!fits)std::fprintf(stderr,"world_library_ui_bounds element=%s viewport=%dx%d box=%.1f,%.1f,%.1f,%.1f\n",
          id,dimensions.x,dimensions.y,offset.x,offset.y,size.x,size.y);
    }
    auto* open=s.library.document->GetElementById("library-open");
    auto* new_save=s.library.document->GetElementById("library-new-save");
    if(open && new_save) {
      const auto open_offset=open->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto new_offset=new_save->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto open_size=open->GetBox().GetSize(Rml::BoxArea::Border);
      const auto new_size=new_save->GetBox().GetSize(Rml::BoxArea::Border);
      expect(new_offset.y>=open_offset.y+open_size.y,"world_actions_do_not_overlap");
      expect(std::abs(new_size.x-open_size.x)<1.f,"world_actions_fill_sidebar_width");
    }
    if(auto* rows=s.library.document->GetElementById("library-list")) {
      auto* wrapper=rows->GetChild(0);
      auto* button=wrapper?wrapper->GetChild(0):nullptr;
      expect(button && button->GetBox().GetSize(Rml::BoxArea::Border).x>=180.f,"world_row_has_readable_width");
      if(button) {
        auto* text=button->QuerySelector(".library-world-text");
        expect(text && text->GetBox().GetSize(Rml::BoxArea::Border).x>=90.f,"world_name_has_readable_width");
      }
    }
  }
  set_world_library({},"",false);s.context->Update();
  expect(s.library.empty && !s.library.has_selection && !s.library.can_open,"empty_library_state");
  auto* empty=s.library.document->GetElementById("library-empty");
  expect(empty && empty->IsVisible(true),"empty_library_guidance_visible");
  s.library=std::move(original);s.rebuild_world_library();
  s.controls.display_menu=original_menu;s.controls.session_active=original_session;
  s.lighting.visible=original_lighting;s.loading_visible=original_loading;s.inventory_open=original_inventory;
  s.controls_open=original_controls;s.fsr_open=original_fsr;
  s.context->SetDimensions(original_dimensions);s.context->SetDensityIndependentPixelRatio(original_density);
  s.sync_menu();s.context->Update();
  expect(s.system.errors==0 && s.system.warnings==0,"rmlui_diagnostics_clean");
  std::fprintf(stderr,"world_library_ui_contract=%s checks=%u failures=%u injected_os_events=0\n",
      failures?"failed":"passed",checks,failures);
  return failures==0;
}
}
