#pragma once
#include <RmlUi/Core.h>
#include <deque>
#include <map>
#include <set>
#include <string>

namespace octaryn::client::ui {
class ModulePanelUi final : public Rml::EventListener {
  Rml::ElementDocument* document_{};
  std::string id_,declaration_;
  std::set<std::string> fields_,actions_;
  std::deque<std::string> pending_;
public:
  ~ModulePanelUi();
  bool present(Rml::Context&,const std::string& declaration,const std::string& values);
  bool hide(const std::string& id);
  bool visible() const;
  bool toggle();
  void focus();
  bool poll(std::string& action);
  void ProcessEvent(Rml::Event&) override;
};
}
