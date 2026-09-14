#include "GameUiState.h"
#include <algorithm>

namespace octaryn::client::app {
bool GameUi::validation_request_item_drop(bool stack) {
  auto& s=*state_;const auto selected=s.inventory.selected_block();
  if(s.drop_request.count || !selected || (s.inventory.cursor().count && s.inventory.cursor().block!=selected))return false;
  const auto count=stack?(s.inventory.cursor().count?s.inventory.cursor().count:
      s.inventory.counts()[s.inventory.selected_slot()]):1u;
  s.request_drop(stack);s.sync_inventory();return count>0&&s.drop_request.count==count;
}
std::uint32_t GameUi::inventory_count(std::uint16_t block) const {
  if(!block)return 0;
  const auto& inventory=state_->inventory;std::uint32_t count{};
  for(unsigned i=0;i<Inventory::SlotCount;++i)if(inventory.slots()[i]==block)count+=inventory.counts()[i];
  if(inventory.cursor().block==block)count+=inventory.cursor().count;
  return count;
}
std::uint64_t GameUi::inventory_drop_watermark() const {return state_->inventory.drop_watermark();}
std::uint64_t GameUi::inventory_grant_watermark() const {return state_->inventory.grant_watermark();}
void GameUi::State::inventory_message(const char* message) {
  text("inventory-status",message);text("inventory-toast",message);
  inventory_toast_until=system.GetElapsedTime()+3;visible("inventory-toast",true);
}
void GameUi::State::position_cursor(float x,float y) {
  cursor_x=x;cursor_y=y;
  if(auto* cursor=document->GetElementById("cursor-item")) {
    cursor->SetProperty(Rml::PropertyId::Left,Rml::Property(x+12,Rml::Unit::PX));
    cursor->SetProperty(Rml::PropertyId::Top,Rml::Property(y+12,Rml::Unit::PX));
  }
  if(auto* tooltip=document->GetElementById("inventory-tooltip")) {
    const auto dimensions=context->GetDimensions();
    tooltip->SetProperty(Rml::PropertyId::Left,Rml::Property(std::min(x+18,std::max(0.f,float(dimensions.x)-240)),Rml::Unit::PX));
    tooltip->SetProperty(Rml::PropertyId::Top,Rml::Property(std::min(y+44,std::max(0.f,float(dimensions.y)-48)),Rml::Unit::PX));
  }
}
void GameUi::State::request_drop(bool stack) {
  if(drop_request.count)return;
  if(!inventory.cursor().count) {
    if(stack)inventory.click_slot(inventory.selected_slot());
    else inventory.right_click_slot(inventory.selected_slot());
  }
  const auto held=inventory.cursor();if(!held.count)return;
  drop_request={held.block,stack?held.count:1u};drop_taken=false;
  if(!inventory.reserve_drop(drop_request.count)){drop_request={};return;}
  inventory_message("Dropping item...");
}
bool GameUi::take_drop_request(GameUiDropRequest& request) {
  auto& s=*state_;if(!s.drop_request.count || s.drop_taken)return false;
  if(s.palette_path.empty() || !s.inventory.save_if_changed(s.palette_path)) {
    s.inventory_message("Drop waiting: inventory save unavailable.");return false;
  }
  request=s.drop_request;s.drop_taken=true;return true;
}
bool GameUi::finish_drop(bool accepted,std::uint64_t command_id) {
  auto& s=*state_;if(!command_id)return false;
  if(command_id<=s.inventory.drop_watermark())return true;
  if(!s.drop_request.count)return false;
  const auto before=s.inventory;s.inventory.resolve_drop(accepted,command_id);
  if(s.palette_path.empty() || !s.inventory.save_if_changed(s.palette_path)) {
    s.inventory=before;s.inventory_message("Drop receipt waiting: inventory save unavailable.");return false;
  }
  s.drop_request={};s.drop_taken=false;
  s.sync_inventory();s.inventory_message(accepted?"Item dropped.":"Drop unavailable. Your items are still held.");
  return true;
}
bool GameUi::apply_pickup(std::uint64_t grant,std::uint16_t block,std::uint32_t count) {
  auto& s=*state_;
  const double now=s.system.GetElapsedTime();
  // The server retains unacknowledged grants. Retry immediately on inventory
  // edits, or at a bounded cadence for external save/capacity failures.
  if(grant==s.failed_pickup && s.inventory.revision()==s.failed_pickup_revision && now<s.pickup_retry_at)return false;
  const auto before=s.inventory;
  if(s.palette_path.empty() || !s.inventory.credit_grant(grant,block,count) || !s.inventory.save_if_changed(s.palette_path)) {
    s.inventory=before;s.failed_pickup=grant;s.failed_pickup_revision=s.inventory.revision();s.pickup_retry_at=now+.5;
    s.inventory_message("Pickup waiting: inventory full or save unavailable.");return false;
  }
  s.failed_pickup=0;
  s.sync_inventory();return true;
}
bool GameUi::State::inventory_pointer(Rml::Event& event,Rml::Element* target) {
  if(!inventory_open)return false;
  const auto type=event.GetType();
  if(type=="mousemove" || type=="drag") {
    position_cursor(event.GetParameter<float>("mouse_x",cursor_x),event.GetParameter<float>("mouse_y",cursor_y));
    return true;
  }
  while(target && !target->HasAttribute("action"))target=target->GetParentNode();
  const auto action=target?target->GetAttribute<Rml::String>("action",""):"";
  const bool slot=action=="inventory-slot" || action=="select-target" || action=="hotbar-slot";
  const unsigned index=target?target->GetAttribute<unsigned>("slot",Inventory::SlotCount):Inventory::SlotCount;
  if(type=="mouseover" && !slot && action!="creative-block")visible("inventory-tooltip",false);
  if(type=="dragstart") {
    if(drop_request.count)return true;
    if(!inventory.cursor().count) {
      if(slot)inventory.click_slot(index);
      else if(action=="creative-block")inventory.take_creative(target->GetAttribute<std::uint16_t>("block",0));
    }
    inventory_dragging=inventory.cursor().count!=0;suppress_inventory_click=inventory_dragging;
    sync_inventory();return true;
  }
  if(type=="dragdrop") {
    if(inventory_dragging && !drop_request.count) {
      if(slot)inventory.click_slot(index);
      else if(!target)request_drop(true);
    }
    inventory_dragging=false;sync_inventory();return true;
  }
  if(type=="dragend") {inventory_dragging=false;suppress_inventory_click=false;return true;}
  if(type=="mousedown") {
    suppress_inventory_click=false;
    if(event.GetParameter<int>("button",0)!=1)return false;
    if(!drop_request.count) {
      if(slot)inventory.right_click_slot(index);
      else if(action=="creative-block")inventory.take_creative(target->GetAttribute<std::uint16_t>("block",0),true);
      else if(!target)request_drop(false);
    }
    sync_inventory();return true;
  }
  if(type=="click") {
    if(event.GetParameter<int>("button",0)!=0 || suppress_inventory_click){suppress_inventory_click=false;return true;}
    if(drop_request.count && (slot || action=="creative-block" || action=="clear-slot" || action=="sort-inventory"))return true;
    if(!target && inventory.cursor().count){request_drop(true);return true;}
  }
  return false;
}
}
