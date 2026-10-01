#include "UiAudioFeedback.h"
#include "ActionAudio.h"
#include <RmlUi/Core.h>

namespace octaryn::client::ui {
namespace {
constexpr const char* events[]={"mouseover","mouseout","focus","click","change","mousedown","dragstart","dragdrop","dragend"};
bool typing(Rml::Element* element) {
  const auto tag=element->GetTagName();
  if(tag=="textarea")return true;
  if(tag!="input")return false;
  const auto type=element->GetAttribute<Rml::String>("type","text");
  return type!="range" && type!="checkbox" && type!="radio" && type!="button" && type!="submit";
}
Rml::Element* control(Rml::Element* element) {
  for(auto* node=element;node;node=node->GetParentNode()) {
    const auto& tag=node->GetTagName();
    if(tag=="button" || tag=="input" || tag=="textarea" || tag=="select" || node->HasAttribute("action") || node->HasAttribute("row"))return node;
  }
  return nullptr;
}
bool enabled(Rml::Element* element) {
  if(!element || !element->IsVisible(true))return false;
  for(auto* node=element;node;node=node->GetParentNode())if(node->HasAttribute("disabled"))return false;
  return true;
}
bool inventory_control(Rml::Element* element) {
  const auto action=element->GetAttribute<Rml::String>("action","");
  return action=="inventory-slot" || action=="hotbar-slot" || action=="select-target" || action=="creative-block";
}
}
UiAudioFeedback::Input::Input(UiAudioFeedback& owner,bool active,bool motion,bool keyboard,double now):owner_(owner) {
  owner_.input_=active;owner_.motion_=motion;owner_.keyboard_=keyboard;owner_.now_=now;owner_.pressed_=nullptr;
}
UiAudioFeedback::Input::~Input() {stop();}
void UiAudioFeedback::Input::stop() {owner_.input_=false;owner_.motion_=false;owner_.keyboard_=false;owner_.pressed_=nullptr;}
UiAudioFeedback::~UiAudioFeedback() {detach();}
void UiAudioFeedback::attach(Rml::Context* context) {
  detach();context_=context;
  if(context_)for(const auto* type:events)context_->AddEventListener(type,this,true);
}
void UiAudioFeedback::detach() {
  if(context_)for(const auto* type:events)context_->RemoveEventListener(type,this,true);
  context_=nullptr;reset();
}
void UiAudioFeedback::set_audio(audio::ActionAudio* value) {audio_=value;reset();}
void UiAudioFeedback::reset() {
  last_hover_=last_activation_=last_change_=pressed_=nullptr;
  hover_at_=activation_at_=change_at_=drop_at_=-1;dragging_=false;counts_={};
}
bool UiAudioFeedback::pointer_moved(float x,float y) {
  const bool moved=!pointer_known_ || pointer_x_!=x || pointer_y_!=y;
  pointer_known_=true;pointer_x_=x;pointer_y_=y;return moved;
}
void UiAudioFeedback::emit(audio::ActionSound sound,Rml::Element* element) {
  if(sound==audio::ActionSound::UiHover) {
    if(last_hover_==element || now_-hover_at_<.065)return;
    last_hover_=element->GetObserverPtr();hover_at_=now_;++counts_.hover;
  } else if(sound==audio::ActionSound::UiClick) {
    if(last_activation_==element && now_-activation_at_<.08)return;
    last_activation_=element?element->GetObserverPtr():nullptr;activation_at_=now_;++counts_.click;
  } else {
    if((last_change_==element && now_-change_at_<.09) || now_-change_at_<.035)return;
    last_change_=element?element->GetObserverPtr():nullptr;change_at_=now_;++counts_.change;
  }
  if(audio_)audio::play_action_audio(audio_,sound);
}
void UiAudioFeedback::activate(Rml::Element* element) {
  if(input_ && enabled(element))emit(audio::ActionSound::UiClick,element);
}
void UiAudioFeedback::transition(bool change) {
  if(input_)emit(change?audio::ActionSound::UiChange:audio::ActionSound::UiClick,nullptr);
}
void UiAudioFeedback::ProcessEvent(Rml::Event& event) {
  const auto& type=event.GetType();
  if(type=="mouseout" && context_ && last_hover_!=control(context_->GetHoverElement()))last_hover_=nullptr;
  if(!input_)return;
  auto* target=event.GetTargetElement();
  auto* element=control(target);
  if(type=="dragend") {dragging_=false;return;}
  if(type=="dragdrop" && dragging_ && target && target->IsVisible(true)) {
    if(element && !enabled(element))return;
    emit(audio::ActionSound::UiChange,element);drop_at_=now_;dragging_=false;return;
  }
  if(!enabled(element))return;
  if(inventory_blocked_ && inventory_control(element) && type!="mouseover" && type!="focus")return;
  if(type=="mousedown")pressed_=element->GetObserverPtr();
  if(type=="mouseover" && motion_ && !dragging_)emit(audio::ActionSound::UiHover,element);
  else if(type=="focus" && keyboard_ && event.GetParameter<bool>("focus_visible",false))emit(audio::ActionSound::UiHover,element);
  else if(type=="change" && !typing(element) &&
      (pressed_==element || (context_ && control(context_->GetFocusElement())==element)))emit(audio::ActionSound::UiChange,element);
  else if(type=="dragstart" && inventory_control(element)) {
    dragging_=true;emit(audio::ActionSound::UiClick,element);
  } else if(type=="mousedown" && event.GetParameter<int>("button",0)==1 && inventory_control(element))
    emit(audio::ActionSound::UiChange,element);
  else if(type=="click" && event.GetParameter<int>("button",0)==0 && !dragging_ && now_-drop_at_>.08) {
    const auto tag=element->GetTagName();
    // Options, sliders, and toggles emit change when committed.
    const auto input_type=element->GetAttribute<Rml::String>("type","text");
    if(tag=="input" && (input_type=="range" || input_type=="checkbox" || input_type=="radio"))return;
    if(tag=="select")for(auto* node=target;node && node!=element;node=node->GetParentNode())if(node->GetTagName()=="option")return;
    emit(audio::ActionSound::UiClick,element);
  }
}
}
