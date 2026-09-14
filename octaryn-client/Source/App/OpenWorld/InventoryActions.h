#pragma once
#include "ActionFeedback.h"
#include "GameUi.h"

namespace octaryn::client::app {
template<class Submit,class Play>
void dispatch_inventory_actions(world_presentation::BlockInteraction& interaction,GameUi& ui,
    const BlockActionQueue& input,Submit&& submit,Play&& play) {
  interaction.select(ui.selected_block());
  for(const auto& action:input.actions()) {
    if(action.kind==BlockActionKind::Select || action.kind==BlockActionKind::Cycle) {
      const bool changed=action.kind==BlockActionKind::Select?
          ui.select_hotbar(static_cast<unsigned>(action.delta)):ui.cycle_hotbar(action.delta);
      interaction.select(ui.selected_block());
      if(changed) play(audio::ActionSound::Change);
    } else if(action.kind==BlockActionKind::Pick) {
      if(interaction.target().hit && ui.pick_block(interaction.target().block_id)) {
        interaction.select(ui.selected_block());play(audio::ActionSound::Select);
      }
    } else if(action.kind==BlockActionKind::Break || action.kind==BlockActionKind::Place) {
      const bool place=action.kind==BlockActionKind::Place;
      world_presentation::BlockEditIntent edit;
      if(interaction.make_edit(place,edit) && submit(edit))
        play(place?audio::ActionSound::Place:audio::ActionSound::Break);
    }
  }
}
}
