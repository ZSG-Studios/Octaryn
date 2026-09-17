#include "GameUiState.h"
#include "Menu.h"
#include <array>

namespace octaryn::client::app {
namespace {
std::string escaped(const std::string& value) {
  std::string result;
  for (char c:value) {
    if(c=='&') result+="&amp;";
    else if(c=='<') result+="&lt;";
    else if(c=='>') result+="&gt;";
    else if(c=='\"') result+="&quot;";
    else result+=c;
  }
  return result;
}
std::string icon(const InventoryBlock* block) {
  if (!block) return "";
  return "<img src=\"../Atlases/basegame-color.png\" rect=\""+
      std::to_string(block->atlas_tile*32)+" 0 32 32\"/>";
}
constexpr std::array category_names={"all","terrain","nature","lighting","fluids"};
}
bool GameUi::State::modal_open() const {
  return inventory_open || controls.display_menu.active || lighting.visible;
}
bool GameUi::modal_open() const { return state_->modal_open(); }
std::uint16_t GameUi::selected_block() const { return state_->inventory.selected_block(); }
bool GameUi::select_hotbar(unsigned slot) { return state_->inventory.select_hotbar(slot); }
bool GameUi::cycle_hotbar(int delta) { return state_->inventory.cycle_hotbar(delta); }
bool GameUi::pick_block(std::uint16_t block) { return !state_->drop_request.count && state_->inventory.pick(block); }
void GameUi::show_inventory(bool creative) { state_->open_inventory(creative); }
void GameUi::show_pause_menu() { state_->open_pause(); }
void GameUi::State::save_inventory() {
  if (!palette_path.empty() && !inventory.save_if_changed(palette_path))
    std::fprintf(stderr,"Inventory palette save failed.\n");
}
void GameUi::State::open_inventory(bool creative) {
  inventory_open=true;creative_open=creative;controls_open=false;fsr_open=false;
  lighting.visible=false;controls.display_menu.active=0;
  sync_inventory();sync_menu();sync_capture();
}
void GameUi::State::close_inventory() {
  if(!drop_request.count)inventory.cancel_move();inventory_open=false;
  save_inventory();sync_menu();sync_capture();
}
void GameUi::State::open_pause() {
  if(!drop_request.count)inventory.cancel_move();inventory_open=false;controls_open=false;fsr_open=false;lighting.visible=false;
  int width{},height{};SDL_GetWindowSizeInPixels(window,&width,&height);
  runtime_controls_refresh_menu(&controls,window,width,height);
  display_menu_open(&controls.display_menu);
  controls.display_menu.screen=DISPLAY_MENU_SCREEN_INGAME;
  controls.display_menu.row=2;
  sync_menu();sync_capture();
}
bool GameUi::State::inventory_action(Rml::Element* target,const Rml::String& action) {
  if (action=="open-inventory") open_inventory(false);
  else if(action=="open-creative") open_inventory(true);
  else if(action=="close-inventory") close_inventory();
  else if(action=="tab-inventory") creative_open=false;
  else if(action=="tab-creative") creative_open=true;
  else if(action=="inventory-slot") inventory.click_slot(target->GetAttribute<unsigned>("slot",Inventory::SlotCount));
  else if(action=="hotbar-slot" || action=="select-target") inventory.select_hotbar(target->GetAttribute<unsigned>("slot",Inventory::HotbarCount));
  else if(action=="creative-block") {
    const int id=target->GetAttribute<int>("block",-1);
    if(id>0 && id<=65535 && inventory.assign(static_cast<std::uint16_t>(id)))
      text("inventory-status","Added to hotbar slot "+std::to_string(inventory.selected_slot()+1)+".");
  } else if(action=="category") {
    const auto category=target->GetAttribute<Rml::String>("category","all");
    for(unsigned i=0;i<category_names.size();++i)
      if(category==category_names[i]) inventory_category=static_cast<InventoryCategory>(i);
    creative_dirty=true;
  } else if(action=="sort-inventory") inventory.sort_backpack();
  else if(action=="clear-slot") {
    const int moving=inventory.moving_slot();
    inventory.clear_slot(moving>=0?static_cast<unsigned>(moving):inventory.selected_slot());
    inventory.cancel_move();
  } else if(action=="open-lighting") {
    inventory_open=false;controls_open=false;controls.display_menu.active=0;lighting.visible=true;
  } else if(action=="open-controls") controls_open=true;
  else if(action=="back-pause") return_to_menu();
  else return false;
  return true;
}
void GameUi::State::inventory_hover(Rml::Element* target) {
  const auto action=target->GetAttribute<Rml::String>("action","");
  const InventoryBlock* block=nullptr;
  if(action=="creative-block") {
    const auto id=target->GetAttribute<int>("block",-1);
    if(id>0 && id<=65535) block=inventory.find(static_cast<std::uint16_t>(id));
  } else if(action=="inventory-slot" || action=="select-target") {
    const auto slot=target->GetAttribute<unsigned>("slot",Inventory::SlotCount);
    if(slot<Inventory::SlotCount) block=inventory.find(inventory.slots()[slot]);
  } else return;
  visible("inventory-tooltip",block && !inventory.cursor().count);
  if(block)text("inventory-tooltip",escaped(block->name));
  if(!inventory.cursor().count)
    text("inventory-status",block?escaped(block->name)+" / Creative block":"Empty slot");
}
void GameUi::State::sync_inventory() {
  if(!slots_initialized) {
    const auto grid=[&](unsigned count,const char* prefix,const char* action) {
      std::string markup;
      for(unsigned i=0;i<count;++i) {
        const auto id=std::string(prefix)+std::to_string(i);
        markup+="<button id=\""+id+"\" action=\""+action+"\" slot=\""+std::to_string(i)+"\" class=\"item-slot\">";
        markup+="<span id=\""+id+"-icon\" class=\"slot-icon\"/><span id=\""+id+"-count\" class=\"slot-count\"/>";
        if(i<Inventory::HotbarCount)markup+="<span class=\"slot-number\">"+std::to_string((i+1)%10)+"</span>";
        markup+="</button>";
      }
      return markup;
    };
    text("inventory-slots",grid(Inventory::SlotCount,"inventory-slot-","inventory-slot"));
    text("hotbar",grid(Inventory::HotbarCount,"hotbar-slot-","hotbar-slot"));
    text("creative-hotbar",grid(Inventory::HotbarCount,"creative-target-","select-target"));
    slots_initialized=true;
  }
  visible("cursor-item",inventory_open && inventory.cursor().count!=0);
  if(!inventory_open || inventory.cursor().count)visible("inventory-tooltip",false);
  if (inventory_revision!=inventory.revision()) {
    inventory_revision=inventory.revision();
    for(unsigned slot=0;slot<Inventory::SlotCount;++slot) {
      const auto* block=inventory.find(inventory.slots()[slot]);
      for(const char* prefix:{"inventory-slot-","hotbar-slot-","creative-target-"}) {
        if(slot>=Inventory::HotbarCount && std::string_view(prefix)!="inventory-slot-")continue;
        const auto id=std::string(prefix)+std::to_string(slot);
        if(auto* element=document->GetElementById(id)) {
          element->SetClass("selected",slot==inventory.selected_slot());
          element->SetClass("held",static_cast<int>(slot)==inventory.moving_slot());
          const auto name=block?block->name:"Empty";
          if(element->GetAttribute<Rml::String>("title","")!=name)element->SetAttribute("title",name);
        }
        text((id+"-icon").c_str(),icon(block));
        text((id+"-count").c_str(),inventory.counts()[slot]>1?std::to_string(inventory.counts()[slot]):"");
      }
    }
    const auto cursor=inventory.cursor();
    text("cursor-icon",icon(inventory.find(cursor.block)));
    text("cursor-count",cursor.count>1?std::to_string(cursor.count):"");
    const auto* selected=inventory.find(inventory.selected_block());
    visible("selected-block",selected!=nullptr);
    text("selected-name",selected?escaped(selected->name):"Empty hand");
    text("equipment-hand-name",inventory.hand().available?escaped(inventory.hand().name):"Unavailable");
    text("equipment-hand-state",inventory.hand().available?"Ready to build":"No tool registered");
    if(cursor.count) {
      const auto* moving=inventory.find(cursor.block);
      text("inventory-status",moving?escaped(moving->name)+" / "+std::to_string(cursor.count):"");
    } else text("inventory-status","Hotbar "+std::to_string(inventory.selected_slot()+1)+": "+
        (selected?escaped(selected->name):"Empty hand"));
  }
  if(auto* tab=document->GetElementById("inventory-tab")) tab->SetClass("selected",!creative_open);
  if(auto* tab=document->GetElementById("creative-tab")) tab->SetClass("selected",creative_open);
  if (!creative_dirty) return;
  creative_dirty=false;
  std::string blocks;
  const auto matches=inventory.search(inventory_query,inventory_category);
  for(const auto id:matches) {
    const auto* block=inventory.find(id);
    if (!block) continue;
    const auto name=escaped(block->name);
    blocks+="<button class=\"item-slot creative-item\" action=\"creative-block\" block=\""+
        std::to_string(id)+"\" title=\""+name+"\">"+icon(block)+"<span class=\"slot-name\">"+name+"</span></button>";
  }
  text("creative-blocks",blocks);
  text("creative-count",std::to_string(matches.size())+" blocks");
  visible("creative-empty",matches.empty());
  for(unsigned i=0;i<category_names.size();++i)
    if(auto* category=document->QuerySelector(std::string("[category=")+category_names[i]+"]"))
      category->SetClass("selected",i==static_cast<unsigned>(inventory_category));
}
}
