#include "GameUiState.h"
#include <RmlUi/Core/ComputedValues.h>

namespace octaryn::client::app {

// Look-target highlight: the authority reports the item under the crosshair;
// the card shows its palette name and icon when a content catalog is loaded,
// otherwise the raw item id. Item id 0 clears the card.
void GameUi::show_item_target(std::uint32_t item_id, std::uint32_t count) {
  auto& s = *state_;
  if (!item_id || !count) {
    s.visible("item-target", false);
    return;
  }
  const auto* block = s.inventory.find(static_cast<std::uint16_t>(item_id));
  s.text("item-target-name",
         (block ? block->name : "Item " + std::to_string(item_id)) +
         (count > 1 ? " ×" + std::to_string(count) : ""));
  if (block) {
    s.text("item-target-icon",
           "<img src=\"../Atlases/basegame-color.png\" rect=\"" +
           std::to_string(block->atlas_tile * 32) + " 0 32 32\"/>");
  } else {
    s.text("item-target-icon", "");
  }
  s.visible("item-target", true);
}

void GameUi::clear_item_target() {
  state_->visible("item-target", false);
}

bool GameUi::validate_item_target_contract() {
  auto& s=*state_;
  auto* card=s.document->GetElementById("item-target");
  auto* name=s.document->GetElementById("item-target-name");
  if (!card || !name) return false;
  clear_item_target();s.context->Update();
  bool valid=card->GetComputedValues().display()==Rml::Style::Display::None;
  show_item_target(1,3);s.context->Update();
  valid=valid && card->GetComputedValues().display()!=Rml::Style::Display::None &&
      name->GetInnerRML().find("3")!=Rml::String::npos;
  clear_item_target();s.context->Update();
  valid=valid && card->GetComputedValues().display()==Rml::Style::Display::None;
  std::printf("item_target_contract=%s hidden_shown_hidden=1\n",valid?"passed":"failed");
  return valid;
}

bool GameUi::take_interact_request() {
  const bool requested = state_->interact_requested;
  state_->interact_requested = false;
  return requested;
}

} // namespace octaryn::client::app
