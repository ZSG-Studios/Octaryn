#pragma once
#include "ActionAudio.h"
#include "BlockInteraction.h"
#include "ActionQueue.h"

namespace octaryn::client::app {
// Consume this frame's already focus/modal-filtered action edges. Feedback means
// the local command was queued; only the server decides whether an edit applies.
template<class Interaction,class Submit,class Play>
void dispatch_block_actions(Interaction& interaction,const BlockActionQueue& input,Submit&& submit,Play&& play) {
  for(const auto& action:input.actions()) {
    if(action.kind==BlockActionKind::Cycle) {
      if(interaction.cycle(action.delta)) play(audio::ActionSound::Change);
    } else if(action.kind==BlockActionKind::Pick) {
      if(interaction.pick()) play(audio::ActionSound::Select);
    } else if(action.kind==BlockActionKind::Break || action.kind==BlockActionKind::Place) {
      const bool place=action.kind==BlockActionKind::Place;
      world_presentation::BlockEditIntent edit;
      if(interaction.make_edit(place,edit) && submit(edit))
        play(place?audio::ActionSound::Place:audio::ActionSound::Break);
    }
  }
}
}
