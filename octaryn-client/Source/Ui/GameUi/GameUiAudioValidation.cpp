#include "GameUiState.h"
#include "ActionAudio.h"
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace octaryn::client::app {
namespace {
struct AudioEventStopper final : Rml::EventListener {
  void ProcessEvent(Rml::Event& event) override {event.StopPropagation();}
};
}
bool GameUi::validate_ui_audio_contract() {
  auto& s=*state_;
  unsigned checks{},failures{};
  auto expect=[&](bool valid,const char* requirement) {
    ++checks;if(!valid){++failures;std::fprintf(stderr,"ui_audio_check=failed requirement=%s\n",requirement);}
  };
  audio::SoundDefinitions definitions;
  for(std::size_t i=0;i<definitions.size();++i)definitions[i]={400.0+110*i,.06,24,2,2};
  audio::ActionAudioOwner output(audio::create_action_audio(definitions,audio::OutputMode::Loopback));
  expect(audio::action_audio_status(output.get()).available,"isolated_loopback_available");
  ui::UiAudioFeedback feedback;feedback.attach(s.context);feedback.set_audio(output.get());
  AudioEventStopper stopper;
  auto fixture=[&](Rml::ElementDocument* document) {
    auto node=document->CreateElement("div");
    node->SetProperty("position","absolute");node->SetProperty("width","240px");
    node->SetInnerRML("<button id='audio-button'><span id='audio-child'>Button</span><b id='audio-sibling'>Sibling</b></button>"
        "<input id='audio-text' type='text'/><input id='audio-range' type='range' min='0' max='10'/>"
        "<input id='audio-toggle' type='checkbox'/><select id='audio-select'><option value='first'>First</option><option value='second'>Second</option></select>"
        "<button id='audio-slot' action='inventory-slot'>Item</button><button id='audio-disabled' disabled>Disabled</button>"
        "<button id='audio-hidden' style='display:none'>Hidden</button>");
    for(const auto* type:{"click","change","mousedown","dragstart","dragdrop","dragend"})node->AddEventListener(type,&stopper);
    return document->AppendChild(std::move(node));
  };
  const bool library_visible=s.library.document && s.library.document->IsVisible();
  if(s.library.document)s.library.document->Show(Rml::ModalFlag::None,Rml::FocusFlag::None);
  auto* game=fixture(s.document);
  auto* library=s.library.document?fixture(s.library.document):nullptr;
  s.context->Update();
  const auto previous_focus=s.context->GetFocusElement()->GetObserverPtr();
  double now=1;
  std::uint64_t energy{},samples_rendered{};
  auto drain=[&] {
    std::array<std::int16_t,audio::ActionSampleCount*2> samples{};
    const bool rendered=audio::render_action_audio_loopback(output.get(),samples);
    if(rendered){++samples_rendered;for(const auto sample:samples)energy+=std::uint64_t(std::abs(int(sample)));}
  };
  auto dispatch=[&](Rml::Element* element,const char* type,bool motion=false,bool keyboard=false,int button=0) {
    now+=.25;
    ui::UiAudioFeedback::Input input(feedback,true,motion,keyboard,now);
    Rml::Dictionary parameters;parameters["button"]=button;parameters["focus_visible"]=keyboard;
    if(element)element->DispatchEvent(type,parameters);
    input.stop();drain();
  };
  auto find=[](Rml::Element* node,const char* id){return node?node->GetElementById(id):nullptr;};
  auto* child=find(game,"audio-child");auto* sibling=find(game,"audio-sibling");
  dispatch(child,"mouseover",true);dispatch(sibling,"mouseover",true);dispatch(child,"mouseover",true);
  expect(feedback.counts().hover==1,"one_hover_for_nested_control_indefinitely");
  dispatch(find(library,"audio-button"),"focus",false,true);
  expect(feedback.counts().hover==2,"root_capture_nonbubbling_second_document_focus");
  dispatch(find(game,"audio-button"),"click");dispatch(find(library,"audio-button"),"click");
  expect(feedback.counts().click==2,"root_capture_click_both_documents_before_stopped_bubble");
  const auto quiet=feedback.counts();
  dispatch(find(game,"audio-disabled"),"click");dispatch(find(game,"audio-hidden"),"click");
  child->DispatchEvent("mouseover",{});find(game,"audio-range")->DispatchEvent("change",{});
  dispatch(find(game,"audio-button"),"focus",false,false);
  expect(feedback.counts().click==quiet.click && feedback.counts().hover==quiet.hover &&
      feedback.counts().change==quiet.change,"disabled_hidden_programmatic_events_silent");
  {ui::UiAudioFeedback::Input input(feedback,false,false,true,now+.1);child->DispatchEvent("click",{});}
  expect(feedback.counts().click==quiet.click,"repeated_keyboard_input_silent");
  expect(feedback.pointer_moved(10,20) && !feedback.pointer_moved(10,20) && feedback.pointer_moved(11,20),
      "stationary_pointer_events_do_not_hover");
  dispatch(find(game,"audio-text"),"click");dispatch(find(game,"audio-text"),"mouseover",true);
  dispatch(find(game,"audio-text"),"change",false,true);
  expect(feedback.counts().click==quiet.click+1 && feedback.counts().hover==quiet.hover+1 &&
      feedback.counts().change==quiet.change,"text_interaction_audible_typing_silent");
  auto* slider=find(game,"audio-range");slider->Focus();
  dispatch(slider,"change");const auto first_change=feedback.counts().change;
  {ui::UiAudioFeedback::Input input(feedback,true,false,false,now+.01);slider->DispatchEvent("change",{});}
  drain();expect(feedback.counts().change==first_change,"slider_changes_throttled");
  dispatch(slider,"click");expect(feedback.counts().click==quiet.click+1,"slider_click_not_duplicate_change");
  auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(find(library,"audio-select"));
  expect(select!=nullptr,"native_dropdown_exists");
  if(select) {
    select->Focus();select->ShowSelectBox();s.context->Update();
    const auto clicks=feedback.counts().click;
    dispatch(select->GetOption(1),"click");
    expect(select->GetSelection()==1 && feedback.counts().change==first_change+1 && feedback.counts().click==clicks,
        "dropdown_stopped_option_click_emits_only_selected_change");
  }
  auto* slot=find(game,"audio-slot");
  dispatch(slot,"mousedown",false,false,1);const auto right=feedback.counts();
  dispatch(slot,"click",false,false,1);
  expect(feedback.counts().change==right.change && feedback.counts().click==right.click,"right_inventory_click_not_doubled");
  dispatch(slot,"dragstart");const auto drag=feedback.counts();dispatch(slot,"click");
  expect(feedback.counts().click==drag.click,"drag_suppresses_release_click");
  dispatch(slot,"dragdrop");dispatch(slot,"dragend");
  expect(feedback.counts().change==drag.change+1,"inventory_drop_feedback");
  feedback.block_inventory(true);const auto blocked=feedback.counts();
  dispatch(slot,"click");dispatch(slot,"mousedown",false,false,1);dispatch(slot,"dragstart");
  expect(feedback.counts().click==blocked.click && feedback.counts().change==blocked.change,"pending_inventory_drop_silent");
  feedback.block_inventory(false);
  {ui::UiAudioFeedback::Input input(feedback,true,false,true,now+.25);feedback.transition();}
  drain();
  {ui::UiAudioFeedback::Input input(feedback,true,false,true,now+.5);feedback.transition(true);}
  drain();
  expect(feedback.counts().click==blocked.click+1 && feedback.counts().change==blocked.change+1,"keyboard_menu_transitions_and_settings_change_cues");
  const auto status=audio::action_audio_status(output.get());const auto counts=feedback.counts();
  expect(status.played==counts.hover+counts.click+counts.change && !status.dropped,"accepted_cues_reach_backend_without_voice_exhaustion");
  expect(energy>0 && samples_rendered>0,"actual_loopback_samples_have_energy");
  feedback.detach();
  s.document->RemoveChild(game);
  if(library)s.library.document->RemoveChild(library);
  if(!library_visible && s.library.document)s.library.document->Hide();
  if(previous_focus)previous_focus->Focus();
  s.context->Update();
  std::fprintf(stderr,"ui_audio_contract=%s checks=%u failures=%u hover=%llu click=%llu change=%llu samples_energy=%llu\n",
      failures?"failed":"passed",checks,failures,static_cast<unsigned long long>(counts.hover),
      static_cast<unsigned long long>(counts.click),static_cast<unsigned long long>(counts.change),static_cast<unsigned long long>(energy));
  return failures==0;
}
}
