#pragma once
#include <RmlUi/Core.h>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include "DeclaredBitmapText.h"
#include "DeclaredDocumentLayout.h"

namespace octaryn::client::ui {
class DeclaredDocumentUi final : public Rml::EventListener {
  struct Screen {
    Rml::ElementDocument* document{};
    std::string declaration;
    std::string active_action;
    bool modal{};
    unsigned logical_width{},logical_height{};
    std::string fit_mode="contain";
    int viewport_width{},viewport_height{};
    std::map<std::string,DocumentBinding> fields;
    std::map<std::string,std::string> actions,values;
    std::map<std::string,double> widths;
    std::map<std::string,BitmapTint> tints;
    std::map<std::string,BitmapFont> bitmap_fonts;
  };
  std::map<std::string,Screen> screens_;
  std::deque<std::pair<std::string,std::string>> pending_;
  bool pointer_input_{};
public:
  ~DeclaredDocumentUi();
  void clear();
  bool present(Rml::Context&,const std::string& declaration,const std::string& values);
  bool reflow();
  void fit(Rml::Context&);
  bool hide(const std::string& id);
  bool modal() const;
  bool poll(std::string& action);
  bool dispatch_action(const std::string& action);
  void pointer_move(Rml::Context&);
  void pointer_position(Rml::Context&,int x,int y,int modifiers);
  void keyboard_input(Rml::Context&);
  bool sync_selection(Rml::Context&);
  void ProcessEvent(Rml::Event&) override;
};
}
