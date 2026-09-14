#pragma once
#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementText.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::app {
// Keep labels, units and row elements alive across 4 Hz telemetry refreshes.
// Replacing their parent RML destroys retained geometry even for unchanged rows.
class DebugMetrics {
public:
  static constexpr size_t Count=19;
  using Values=std::array<uint32_t,Count>;
  struct Row {const char* label;const char* unit;unsigned scale;};
  inline static constexpr std::array<Row,Count> Rows{{
    {"Frame","ms",100},{"Average","ms",100},{"Average FPS","fps",10},{"1% low","fps",10},
    {"0.1% low","fps",10},{"Low x5","fps",10},{"Low x10","fps",10},{"Worst","fps",10},
    {"1% time","ms",100},{"0.1% time","ms",100},{"Low x5 time","ms",100},{"Low x10 time","ms",100},
    {"Worst time","ms",100},{"CPU load","%",100},{"GPU load","%",100},{"RAM","GiB",100},
    {"VRAM","GiB",100},{"World","ms",100},{"Render","ms",100}}};
  static std::string format(uint32_t value,unsigned scale) {
    if(value==UINT32_MAX)return "N/A";
    char text[48];std::snprintf(text,sizeof(text),"%.2f",double(value)/scale);return text;
  }
  static std::string markup(const Values& values) {
    std::string rml;
    for(size_t i=0;i<Count;++i)rml+=std::string("<div class='metric'><span>")+Rows[i].label+
      "</span><b>"+format(values[i],Rows[i].scale)+" <small>"+Rows[i].unit+"</small></b></div>";
    return rml;
  }
  void update(Rml::Element* parent,const Values& values) {
    if(!parent)return;
    if(!initialized_) {
      parent->SetInnerRML(markup(values));
      for(size_t i=0;i<Count;++i) {
        auto* row=parent->GetChild(static_cast<int>(i));
        auto* value=row?row->GetChild(1):nullptr;
        auto* text=value?value->GetChild(0):nullptr;
        if(!text || text->GetTagName()!="#text")throw std::runtime_error("Invalid retained debug metric row");
        text_[i]=static_cast<Rml::ElementText*>(text);
      }
      previous_=values;initialized_=true;return;
    }
    for(size_t i=0;i<Count;++i)if(values[i]!=previous_[i]) {
      text_[i]->SetText(format(values[i],Rows[i].scale)+" ");previous_[i]=values[i];
    }
  }
private:
  std::array<Rml::ElementText*,Count> text_{};
  Values previous_{};
  bool initialized_{};
};
}
