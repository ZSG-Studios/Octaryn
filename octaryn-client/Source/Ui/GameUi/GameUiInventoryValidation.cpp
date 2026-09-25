#include "GameUiState.h"
#include "Menu.h"
#include <RmlUi/Core/ComputedValues.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace octaryn::client::app {
bool GameUi::validate_inventory_contract() {
  auto& s=*state_;
  unsigned checks{},failures{};
  auto expect=[&](bool valid,const char* requirement,const std::string& selector) {
    ++checks;
    if (!valid) {
      ++failures;
      std::fprintf(stderr,"rml_ui_inventory_check=failed requirement=%s element=%s\n",requirement,selector.c_str());
    }
  };
  if (!s.document || !s.context || s.inventory.blocks().empty()) {
    std::fprintf(stderr,"rml_ui_inventory_contract=failed reason=missing_document_or_catalog\n");
    return false;
  }
  const auto original_inventory=s.inventory;
  const auto original_menu=s.controls.display_menu;
  const auto original_path=s.palette_path;
  const auto original_query=s.inventory_query;
  const auto original_category=s.inventory_category;
  const auto original_dimensions=s.context->GetDimensions();
  const auto original_density=s.context->GetDensityIndependentPixelRatio();
  const auto original_screen=s.previous_screen,original_overlay=s.previous_overlay;
  const auto original_pending=s.pending;
  const auto original_drop=s.drop_request;const bool original_drop_taken=s.drop_taken;
  auto* toast=s.document->GetElementById("inventory-toast");
  const auto original_toast=toast?toast->GetInnerRML():Rml::String{};
  const bool original_toast_visible=toast&&toast->IsVisible(true);
  const auto original_toast_until=s.inventory_toast_until;
  const bool original_drag=s.inventory_dragging,original_suppressed=s.suppress_inventory_click;
  const float original_cursor_x=s.cursor_x,original_cursor_y=s.cursor_y;
  auto* creative_scroll=s.document->QuerySelector(".creative-results");
  const float original_scroll=creative_scroll?creative_scroll->GetScrollTop():0.f;
  const auto original_restore=s.controls.restore_relative_mouse_after_ui;
  const auto original_session=s.controls.session_active;
  const bool original_open=s.inventory_open,original_creative=s.creative_open,original_controls=s.controls_open;
  const bool original_lighting=s.lighting.visible,original_compact=s.document->IsClassSet("compact");
  const bool original_modal=s.modal_was_open,original_mouse=s.mouse_was_relative;
  const bool original_lighting_was=s.lighting_was_visible,original_release=s.release_input_pending;
  const bool original_relative=SDL_GetWindowRelativeMouseMode(s.window);
  const auto* focused=s.context->GetFocusElement();
  const std::string original_focus=focused?focused->GetId():"";
  // Contract actions must never save their temporary palette to the player's world.
  s.palette_path.clear();
  s.drop_request={};s.drop_taken=false;s.inventory.resolve_drop(false);
  s.controls.session_active=1;
  s.inventory_query.clear();s.inventory_category=InventoryCategory::All;s.creative_dirty=true;
  auto refresh=[&] {s.sync_menu();s.sync_inventory();s.context->Update();};
  auto query=[&](const std::string& selector) {
    auto* result=s.document->QuerySelector(selector);
    expect(result!=nullptr,"required_control",selector);
    return result;
  };
  auto click=[&](const std::string& selector) {
    if (auto* control=query(selector)) control->DispatchEvent("click",{});
    refresh();
  };
  auto search=[&](const std::string& value) {
    if (auto* control=query("#creative-search")) {
      control->SetAttribute("value",value);
      Rml::Dictionary parameters;parameters["value"]=value;
      control->DispatchEvent("change",parameters);
    }
    refresh();
  };
  auto key=[&](SDL_Keycode code,SDL_Scancode scan) {
    SDL_Event input{};input.type=SDL_EVENT_KEY_DOWN;input.key.key=code;input.key.scancode=scan;
    const auto dimensions=s.context->GetDimensions();
    const auto flags=event(input,dimensions.x,dimensions.y);
    refresh();return flags;
  };
  auto count=[&](const std::string& selector) {
    Rml::ElementList elements;s.document->QuerySelectorAll(elements,selector);return elements.size();
  };
  refresh();
  expect(count("#hud .hints")==0,"hud_key_helpers_removed","#hud");
  expect(count("[action=hotbar-slot]")==Inventory::HotbarCount,"hotbar_slot_count","#hotbar");
  expect(count("[action=inventory-slot]")==Inventory::SlotCount,"inventory_slot_count","#inventory-slots");
  for (unsigned i=0;i<Inventory::SlotCount;++i)s.inventory.clear_slot(i);
  s.inventory.select_hotbar(0);
  const auto first=s.inventory.blocks().front().id;
  const auto last=s.inventory.blocks().back().id;
  s.inventory.assign(first);
  show_inventory();refresh();
  expect(modal_open() && s.inventory_open && !s.creative_open,"inventory_modal_open","#inventory");
  click("#creative-tab");expect(s.creative_open,"creative_tab_navigation","#creative-tab");
  click("#inventory-tab");expect(!s.creative_open,"inventory_tab_navigation","#inventory-tab");
  click("[action=inventory-slot][slot=0]");
  expect(s.inventory.moving_slot()==0,"slot_pickup_from_dom","#inventory-slots");
  click("[action=inventory-slot][slot=12]");
  expect(s.inventory.slots()[0]==0 && s.inventory.slots()[12]==first && s.inventory.moving_slot()<0,
      "slot_move_from_dom","#inventory-slots");
  click("[action=inventory-slot][slot=12]");
  click("[action=inventory-slot][slot=0]");
  expect(s.inventory.slots()[0]==first && s.inventory.slots()[12]==0,"slot_move_reversible","#inventory-slots");
  // Feed domain mouse coordinates directly to RmlUi, never SDL/OS input.
  const auto drag=[&](unsigned from,unsigned to) {
    auto* source=query("#inventory-slot-"+std::to_string(from));
    auto* destination=query("#inventory-slot-"+std::to_string(to));
    if(!source || !destination)return;
    const auto a=source->GetAbsoluteOffset(Rml::BoxArea::Border),b=destination->GetAbsoluteOffset(Rml::BoxArea::Border);
    s.context->ProcessMouseMove(int(a.x+24),int(a.y+24),0);s.context->ProcessMouseButtonDown(0,0);
    s.context->ProcessMouseMove(int(a.x+31),int(a.y+24),0);
    s.context->ProcessMouseMove(int(b.x+24),int(b.y+24),0);s.context->ProcessMouseButtonUp(0,0);refresh();
    expect(s.document->GetElementById("inventory-slot-"+std::to_string(from))==source,
        "drag_preserves_retained_source_element","#inventory-slots");
  };
  drag(0,12);
  expect(s.inventory.slots()[0]==0 && s.inventory.slots()[12]==first && !s.inventory.cursor().count,
      "real_drag_drop_transfers_stack","#inventory-slots");
  drag(12,0);
  expect(s.inventory.slots()[0]==first && !s.inventory.cursor().count,"real_drag_drop_reversible","#inventory-slots");
  if(auto* slot=query("#inventory-slot-0")) {
    Rml::Dictionary parameters;parameters["button"]=1;slot->DispatchEvent("mousedown",parameters);refresh();
    expect(s.inventory.cursor().count==1 && s.inventory.counts()[0]==Inventory::StackLimit-1,
        "right_click_takes_one","#inventory-slot-0");
    slot->DispatchEvent("mousedown",parameters);refresh();
    expect(!s.inventory.cursor().count && s.inventory.counts()[0]==Inventory::StackLimit,
        "right_click_returns_one","#inventory-slot-0");
  }
  {
    SDL_Event toss{};toss.type=SDL_EVENT_KEY_DOWN;toss.key.key=SDLK_T;
    toss.key.scancode=SDL_SCANCODE_T;toss.key.mod=SDL_KMOD_CTRL;
    const auto dimensions=s.context->GetDimensions();
    event(toss,dimensions.x,dimensions.y);refresh();
    expect(s.drop_request.block_id==first && s.drop_request.count==999 &&
        s.inventory.reserved_drop()==999 && s.inventory.cursor().count==999 && !s.inventory.counts()[0],
        "ctrl_t_reserves_entire999_stack_without_truncation","#inventory-slot-0");
    s.inventory.resolve_drop(false);s.drop_request={};s.drop_taken=false;
    s.inventory.cancel_move();refresh();
    expect(s.inventory.counts()[0]==999 && !s.inventory.cursor().count,
        "whole_stack_rejection_returns_every_unit","#inventory-slot-0");
  }
  if (auto* slot=query("#inventory-slot-0"))slot->Focus();
  click("#inventory-slot-0");
  const auto focus_id=[&] {
    const auto* control=s.context->GetFocusElement();return control?control->GetId():Rml::String{};
  };
  expect(focus_id()=="inventory-slot-0","slot_focus_survives_rebuild","#inventory-slot-0");
  s.context->ProcessKeyDown(Rml::Input::KI_TAB,0);refresh();
  expect(focus_id()=="inventory-slot-1","tab_navigates_after_pickup","#inventory-slot-1");
  s.context->ProcessKeyDown(Rml::Input::KI_RETURN,0);refresh();
  expect(s.inventory.slots()[0]==0 && s.inventory.slots()[1]==first && s.inventory.moving_slot()<0,
      "enter_places_in_focused_slot","#inventory-slot-1");
  expect(focus_id()=="inventory-slot-1","keyboard_focus_survives_rebuild","#inventory-slot-1");
  click("#inventory-slot-1");click("#inventory-slot-0");
  click("[action=inventory-slot][slot=0]");
  expect((key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE)&RUNTIME_CONTROLS_EVENT_CAPTURED)!=0,
      "inventory_escape_captured","#inventory");
  expect(!modal_open() && s.inventory.moving_slot()<0 && s.inventory.slots()[0]==first,
      "inventory_close_cancels_move","#inventory");

  show_inventory(true);refresh();
  expect(s.creative_open && modal_open(),"creative_modal_open","#creative-page");
  if (auto* input=query("#creative-search"))input->Focus();
  expect((key(SDLK_E,SDL_SCANCODE_E)&RUNTIME_CONTROLS_EVENT_CAPTURED)!=0 && s.inventory_open && s.creative_open,
      "search_typing_does_not_toggle_inventory","#creative-search");
  search("zzzz-no-block-matches-this-query");
  expect(count("[action=creative-block]")==0,"search_no_results","#creative-blocks");
  if (auto* empty=query("#creative-empty"))expect(empty->IsVisible(true),"empty_state_visible","#creative-empty");
  auto exact=s.inventory.blocks().back().key;
  std::transform(exact.begin(),exact.end(),exact.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
  search(exact);
  expect(count("[action=creative-block]")==1,"search_case_insensitive_key","#creative-search");
  expect(count("#creative-hotbar [action=select-target]")==Inventory::HotbarCount,
      "creative_target_count","#creative-hotbar");
  click("#creative-hotbar [action=select-target][slot=7]");
  expect(s.inventory.selected_slot()==7,"creative_target_selection","#creative-hotbar");
  click("[action=creative-block][block="+std::to_string(last)+"]");
  expect(s.inventory.selected_slot()==7 && s.inventory.slots()[7]==last && selected_block()==last,
      "creative_assigns_visible_target","#creative-blocks");
  search("");
  constexpr std::array categories={"all","terrain","nature","lighting","fluids"};
  for (unsigned i=0;i<categories.size();++i) {
    click(std::string("[action=category][category=")+categories[i]+"]");
    const auto category=static_cast<InventoryCategory>(i);
    const auto expected=std::count_if(s.inventory.blocks().begin(),s.inventory.blocks().end(),[&](const auto& block) {
      return category==InventoryCategory::All || block.category==category;
    });
    expect(count("[action=creative-block]")==static_cast<std::size_t>(expected),
        "category_filters_catalog",categories[i]);
  }
  click("[action=category][category=all]");
  key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
  click("[action=hotbar-slot][slot=3]");
  expect(s.inventory.selected_slot()==3,"hotbar_select_from_dom","#hotbar");
  if (auto* image=query("#selected-block"))expect(!image->IsVisible(true),"empty_hotbar_hides_held_icon","#selected-block");
  click("[action=hotbar-slot][slot=7]");
  expect(s.inventory.selected_slot()==7 && selected_block()==last,"filled_hotbar_restores_selection","#hotbar");
  if (auto* image=query("#selected-block"))expect(image->IsVisible(true),"filled_hotbar_shows_held_icon","#selected-block");
  expect((key(SDLK_E,SDL_SCANCODE_E)&RUNTIME_CONTROLS_EVENT_CAPTURED)!=0 && s.inventory_open,
      "inventory_shortcut_opens","#inventory");
  key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
  show_pause_menu();refresh();
  expect(s.controls.display_menu.active && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_INGAME,
      "pause_menu_opens","#pause-screen");
  click("[action=open-inventory]");
  expect(s.inventory_open && !s.creative_open && !s.controls.display_menu.active,
      "pause_inventory_navigation","#inventory");
  show_pause_menu();refresh();click("[action=open-creative]");
  expect(s.inventory_open && s.creative_open && !s.controls.display_menu.active,
      "pause_creative_navigation","#creative-page");
  show_pause_menu();refresh();click("[action=open-controls]");
  expect(s.controls_open && modal_open(),"pause_controls_navigation","#controls-screen");
  click("[action=back-pause]");
  expect(!s.controls_open && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_INGAME,
      "controls_return_to_pause","#pause-screen");
  click("#pause-screen [row=4]");
  expect(s.controls.display_menu.active && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_SETTINGS,
      "pause_settings_navigation","#settings-screen");
  const auto upscale=s.controls.display_menu.upscaler_mode;
  for(unsigned i=1;i<=7;++i) {
    click("[action=fsr-mode]");expect(s.controls.display_menu.upscaler_mode==(upscale+i)%7,
        "upscaler_staged_mode_cycle","[action=fsr-mode]");
  }
  click("#settings-screen [row=13]");
  expect(s.controls.display_menu.active && s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_INGAME,
      "settings_return_to_pause","#pause-screen");
  click("#pause-screen [action=open-lighting]");
  expect(s.lighting.visible && modal_open() && !s.controls.display_menu.active,
      "pause_lighting_navigation","#lighting");
  click("#close-lighting");
  expect(!s.lighting.visible && s.controls.display_menu.active &&
      s.controls.display_menu.screen==DISPLAY_MENU_SCREEN_INGAME,"lighting_return_to_pause","#pause-screen");
  click("#pause-screen [row=2]");
  expect(!modal_open(),"pause_resume_closes_ui","#pause-screen");
  show_pause_menu();refresh();s.pending=0;
  click("#pause-screen [row=14]");
  expect((s.pending&RUNTIME_CONTROLS_QUIT_REQUESTED)!=0 && !modal_open(),
      "pause_exit_requests_quit","#pause-screen");
  s.pending=0;

  auto fits=[&](const std::string& selector,Rml::Vector2i dimensions,bool focusable=false) {
    auto* control=query(selector);if (!control)return;
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
    expect(control->IsVisible(true) && size.x>0 && size.y>0,"visible_layout",selector);
    const bool bounded=std::isfinite(offset.x) && std::isfinite(offset.y) &&
        std::isfinite(size.x) && std::isfinite(size.y) && offset.x>=-1 && offset.y>=-1 &&
        offset.x+size.x<=static_cast<float>(dimensions.x)+1.f &&
        offset.y+size.y<=static_cast<float>(dimensions.y)+1.f;
    expect(bounded,"viewport_bounds",selector);
    if (!bounded)std::fprintf(stderr,"rml_ui_inventory_bounds element=%s viewport=%dx%d box=%.1f,%.1f,%.1f,%.1f\n",
        selector.c_str(),dimensions.x,dimensions.y,offset.x,offset.y,size.x,size.y);
    if (focusable) {
      const auto& style=control->GetComputedValues();
      expect(style.focus()==Rml::Style::Focus::Auto && style.tab_index()==Rml::Style::TabIndex::Auto,
          "keyboard_focus_eligible",selector);
    }
  };
  const std::array sizes={Rml::Vector2i{640,480},Rml::Vector2i{1280,720},Rml::Vector2i{1920,1080},Rml::Vector2i{2560,1440}};
  for (const auto dimensions:sizes) {
    s.context->SetDimensions(dimensions);
    s.context->SetDensityIndependentPixelRatio(dimensions.y>=1440?2.f:1.f);
    s.document->SetClass("compact",dimensions.y<700);
    show_inventory();refresh();
    fits("#inventory",dimensions);fits("#inventory-tab",dimensions,true);fits("#creative-tab",dimensions,true);
    fits(".equipment-panel",dimensions);
    fits("#equipment-hand-name",dimensions);fits("#equipment-hand-state",dimensions);
    fits(".close-inventory",dimensions,true);fits("[action=sort-inventory]",dimensions,true);
    fits("[action=clear-slot]",dimensions,true);fits("#inventory-status",dimensions);
    expect(count(".armor-slots")==0 && count(".accessory-slots")==0,"unimplemented_equipment_not_presented","#inventory");
    if(auto* grid=query("#inventory")) {
      const auto offset=grid->GetAbsoluteOffset(Rml::BoxArea::Border);
      expect(offset.x<float(dimensions.x)*.1f && offset.y<float(dimensions.y)*.1f,"inventory_anchored_upper_left","#inventory");
    }
    for (unsigned i=0;i<Inventory::SlotCount;++i)
      fits("[action=inventory-slot][slot="+std::to_string(i)+"]",dimensions,true);
    show_inventory(true);refresh();
    fits("#inventory",dimensions);fits("#inventory-tab",dimensions,true);fits("#creative-tab",dimensions,true);
    fits(".close-inventory",dimensions,true);fits("#inventory-status",dimensions);
    fits("#creative-search",dimensions,true);fits(".creative-results",dimensions);
    if (auto* scrollport=query(".creative-results")) {
      scrollport->SetScrollTop(0);s.context->Update();
      const bool overflows=scrollport->GetScrollHeight()>scrollport->GetClientHeight()+1;
      for (const auto block:{last,first}) {
        const auto selector="[action=creative-block][block="+std::to_string(block)+"]";
        if (auto* control=query(selector)) {
          if (block==last)control->ScrollIntoView({Rml::ScrollAlignment::End,
              Rml::ScrollAlignment::Nearest,Rml::ScrollBehavior::Instant,Rml::ScrollParentage::Closest});
          else scrollport->SetScrollTop(0);
          s.context->Update();
          if (block==last)expect(!overflows || scrollport->GetScrollTop()>0,
              "creative_catalog_scrolls_to_end",selector);
          fits(selector,dimensions,true);
          const auto offset=control->GetAbsoluteOffset(Rml::BoxArea::Border);
          const auto size=control->GetBox().GetSize(Rml::BoxArea::Border);
          const auto parent=scrollport->GetAbsoluteOffset(Rml::BoxArea::Border);
          const float left=parent.x+scrollport->GetClientLeft(),top=parent.y+scrollport->GetClientTop();
          const bool reachable=offset.x>=left-1 && offset.y>=top-1 &&
              offset.x+size.x<=left+scrollport->GetClientWidth()+1 &&
              offset.y+size.y<=top+scrollport->GetClientHeight()+1;
          expect(reachable,"creative_item_reachable_inside_scrollport",selector);
          if (!reachable)std::fprintf(stderr,
              "rml_ui_inventory_scroll element=%s viewport=%dx%d scroll=%.1f item=%.1f,%.1f,%.1f,%.1f client=%.1f,%.1f,%.1f,%.1f\n",
              selector.c_str(),dimensions.x,dimensions.y,scrollport->GetScrollTop(),
              offset.x,offset.y,size.x,size.y,left,top,scrollport->GetClientWidth(),scrollport->GetClientHeight());
        }
      }
    }
    fits("#creative-hotbar",dimensions);
    for (unsigned i=0;i<Inventory::HotbarCount;++i)
      fits("#creative-hotbar [action=select-target][slot="+std::to_string(i)+"]",dimensions,true);
    for (const auto* category:categories)fits(std::string("[action=category][category=")+category+"]",dimensions,true);
    show_pause_menu();refresh();
    fits(dimensions.x==640?"#menu":"#pause-screen",dimensions);
    for (const auto* action:{"open-inventory","open-creative","open-controls"})
      fits(std::string("[action=")+action+"]",dimensions,true);
    click("[action=open-controls]");fits(dimensions.x==640?"#menu":"#controls-screen",dimensions);fits("[action=back-pause]",dimensions,true);
    click("[action=back-pause]");key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    fits("#hotbar",dimensions);fits("#selected-block",dimensions);fits("#selected-name",dimensions);
    for (unsigned i=0;i<Inventory::HotbarCount;++i)
      fits("[action=hotbar-slot][slot="+std::to_string(i)+"]",dimensions);
  }
  s.inventory=original_inventory;s.palette_path=original_path;
  s.inventory_query=original_query;s.inventory_category=original_category;
  s.inventory_open=original_open;s.creative_open=original_creative;s.controls_open=original_controls;
  s.controls.display_menu=original_menu;s.lighting.visible=original_lighting;
  s.pending=original_pending;s.controls.restore_relative_mouse_after_ui=original_restore;
  s.drop_request=original_drop;s.drop_taken=original_drop_taken;
  s.inventory_dragging=original_drag;s.suppress_inventory_click=original_suppressed;
  s.position_cursor(original_cursor_x,original_cursor_y);
  s.controls.session_active=original_session;
  s.context->SetDimensions(original_dimensions);s.context->SetDensityIndependentPixelRatio(original_density);
  s.document->SetClass("compact",original_compact);
  s.inventory_revision=~std::uint64_t{0};s.creative_dirty=true;
  refresh();s.sync_lighting();
  s.text("inventory-toast",original_toast);s.inventory_toast_until=original_toast_until;
  s.visible("inventory-toast",original_toast_visible);
  s.context->Update();
  expect(!toast || (toast->GetInnerRML()==original_toast && toast->IsVisible(true)==original_toast_visible),
      "contract_restores_drop_toast","#inventory-toast");
  if (creative_scroll)creative_scroll->SetScrollTop(original_scroll);
  if (auto* search_input=s.document->GetElementById("creative-search"))search_input->SetAttribute("value",original_query);
  if (!original_focus.empty())if (auto* control=s.document->GetElementById(original_focus))control->Focus();
  s.previous_screen=original_screen;s.previous_overlay=original_overlay;
  s.modal_was_open=original_modal;s.mouse_was_relative=original_mouse;
  s.lighting_was_visible=original_lighting_was;s.release_input_pending=original_release;
  SDL_SetWindowRelativeMouseMode(s.window,original_relative);
  expect(s.system.errors==0 && s.system.warnings==0,"rmlui_diagnostics","document");
  std::fprintf(stderr,"rml_ui_inventory_contract=%s checks=%u failures=%u viewports=4 os_events_injected=0\n",
      failures?"failed":"passed",checks,failures);
  return failures==0;
}
}
