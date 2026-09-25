#include "ActionFeedback.h"
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {
using namespace octaryn::client;
using audio::ActionSound;
using app::BlockActionKind;
struct Interaction {
  bool cycle_valid{},pick_valid{},break_valid{},place_valid{};
  unsigned cycles{},picks{},breaks{},places{};
  int selected{25};
  bool cycle(int delta) {
    ++cycles;
    if(!delta || !cycle_valid) return false;
    selected+=delta;return true;
  }
  bool pick() {++picks;return pick_valid;} // Valid pick may already be selected.
  bool make_edit(bool place,world_presentation::BlockEditIntent& edit) {
    if(place) ++places;else ++breaks;
    edit.block=static_cast<std::uint16_t>(place?selected:0);return place?place_valid:break_valid;
  }
};
void require(bool value,const char* reason) {if(!value) throw std::runtime_error(reason);}
app::BlockActionQueue queue(std::initializer_list<app::BlockAction> actions) {
  app::BlockActionQueue result;
  for(auto action:actions) require(result.push(action),"fixture must fit queue");
  return result;
}
}
void check_action_feedback() {
  constexpr app::BlockAction cycle{BlockActionKind::Cycle,1},pick{BlockActionKind::Pick},
      remove{BlockActionKind::Break},place{BlockActionKind::Place};
  Interaction interaction;
  std::vector<ActionSound> played;
  std::vector<std::uint16_t> submitted;
  bool accept_break=false,accept_place=false;
  auto submit=[&](const world_presentation::BlockEditIntent& edit) {
    submitted.push_back(edit.block);return edit.block==0?accept_break:accept_place;
  };
  auto play=[&](ActionSound sound) {played.push_back(sound);};
  auto dispatch=[&](const app::BlockActionQueue& input) {app::dispatch_block_actions(interaction,input,submit,play);};
  auto reset=[&] {played.clear();submitted.clear();};
  dispatch({});
  require(played.empty() && submitted.empty() && interaction.cycles==0 && interaction.picks==0 &&
      interaction.breaks==0 && interaction.places==0,"idle must produce no operation or feedback");
  dispatch(queue({cycle,pick,remove,place}));
  require(played.empty() && submitted.empty(),"invalid selection/target must never submit or sound");
  interaction={true,true,true,true};
  dispatch(queue({pick}));
  require(played==std::vector{ActionSound::Select} && interaction.selected==25,
      "valid already-selected pick must sound once");
  reset();dispatch(queue({remove,place}));
  require(played.empty() && submitted.size()==2,"rejected edit queue must stay silent");
  reset();accept_break=true;dispatch(queue({remove,place}));
  require(played==std::vector{ActionSound::Break} && submitted.size()==2,"only accepted break gets feedback");
  reset();accept_break=false;accept_place=true;dispatch(queue({remove,place}));
  require(played==std::vector{ActionSound::Place} && submitted.size()==2,"only accepted place gets feedback");

  reset();accept_break=true;
  interaction.selected=25;dispatch(queue({place,cycle}));
  require(submitted==std::vector<std::uint16_t>{25} && interaction.selected==26 &&
      played==std::vector{ActionSound::Place,ActionSound::Change},
      "place before wheel must use selection at click time");
  reset();interaction.selected=25;dispatch(queue({cycle,place}));
  require(submitted==std::vector<std::uint16_t>{26} &&
      played==std::vector{ActionSound::Change,ActionSound::Place},
      "wheel before place must use updated selection");
  reset();dispatch(queue({place,place,remove,remove,pick,pick}));
  require(submitted==std::vector<std::uint16_t>{26,26,0,0} &&
      played==std::vector{ActionSound::Place,ActionSound::Place,ActionSound::Break,
          ActionSound::Break,ActionSound::Select,ActionSound::Select},
      "repeated clicks must remain distinct and ordered");

  auto bounded=queue({});
  for(std::size_t i=0;i<app::BlockActionQueue::capacity/2;++i) {
    require(bounded.push(cycle) && bounded.push(place),"accepted prefix must fit capacity");
  }
  require(bounded.actions().size()==64 && !bounded.overflowed() && bounded.edit_requested(),
      "full queue must retain all 64 accepted actions and frame attack flag");
  require(!bounded.push(remove) && !bounded.push(cycle) && bounded.overflowed() &&
      bounded.actions().size()==64,"overflow must preserve bounded prefix");
  reset();interaction.selected=25;dispatch(bounded);
  require(submitted.size()==32 && played.size()==64 && interaction.selected==57,
      "overflow must not execute dropped suffix");
  for(std::size_t i=0;i<submitted.size();++i) {
    require(submitted[i]==static_cast<std::uint16_t>(26+i) &&
        played[2*i]==ActionSound::Change && played[2*i+1]==ActionSound::Place,
        "accepted capacity prefix must retain exact selection and feedback order");
  }
  bounded.clear();reset();dispatch(bounded);
  require(bounded.actions().empty() && !bounded.overflowed() && !bounded.edit_requested() &&
      submitted.empty() && played.empty(),"next frame or noninteractive clear must not replay actions");
  require(bounded.push({BlockActionKind::Cycle,0}) && bounded.actions().empty(),
      "zero wheel must not consume capacity or emit feedback");
  for(std::size_t i=0;i<app::BlockActionQueue::capacity;++i) bounded.push(pick);
  require(!bounded.push(remove) && bounded.edit_requested(),
      "a gameplay edit input must retain one frame attack indication even when queue is full");
}
