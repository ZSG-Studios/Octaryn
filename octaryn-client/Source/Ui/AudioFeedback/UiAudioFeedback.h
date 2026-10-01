#pragma once
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/ObserverPtr.h>
#include <cstdint>

namespace Rml { class Context; class Element; }
namespace octaryn::client::audio { struct ActionAudio; enum class ActionSound; }
namespace octaryn::client::ui {
struct UiAudioCounts { std::uint64_t hover{},click{},change{}; };
class UiAudioFeedback final : public Rml::EventListener {
public:
  class Input final {
  public:
    Input(UiAudioFeedback& owner,bool active,bool motion,bool keyboard,double now);
    ~Input();
    void stop();
  private:
    UiAudioFeedback& owner_;
  };
  ~UiAudioFeedback();
  void attach(Rml::Context* context);
  void detach();
  void set_audio(audio::ActionAudio* audio);
  void ProcessEvent(Rml::Event& event) override;
  void activate(Rml::Element* element);
  void transition(bool change=false);
  bool pointer_moved(float x,float y);
  void block_inventory(bool blocked) {inventory_blocked_=blocked;}
  UiAudioCounts counts() const { return counts_; }
  audio::ActionAudio* audio() const { return audio_; }
  void reset();
private:
  void emit(audio::ActionSound sound,Rml::Element* control);
  Rml::Context* context_{};
  audio::ActionAudio* audio_{};
  Rml::ObserverPtr<Rml::Element> last_hover_,last_activation_,last_change_,pressed_;
  UiAudioCounts counts_;
  double now_{},hover_at_{-1},activation_at_{-1},change_at_{-1},drop_at_{-1};
  bool input_{},motion_{},keyboard_{},dragging_{};
  bool pointer_known_{};
  bool inventory_blocked_{};
  float pointer_x_{},pointer_y_{};
};
}
