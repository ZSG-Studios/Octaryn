#include "GameUiState.h"

namespace octaryn::client::app {
bool GameUi::present_module_screen(const std::string& declaration,const std::string& fields) {
  auto& s=*state_;
  if(s.module_documents.present(*s.context,declaration,fields)) {
    s.sync_capture();s.context->Update();return true;
  }
  if(!s.module_panel)s.module_panel=std::make_unique<ui::ModulePanelUi>();
  const bool result=s.module_panel->present(*s.context,declaration,fields);
  if(result) {s.sync_capture();s.context->Update();}
  return result;
}
bool GameUi::hide_module_screen(const std::string& id) {
  auto& s=*state_;
  if(s.module_documents.hide(id)){s.sync_capture();return true;}
  if(!s.module_panel || !s.module_panel->hide(id))return false;
  s.sync_capture();return true;
}
bool GameUi::poll_module_screen_action(std::string& action) {
  return state_->module_documents.poll(action) || (state_->module_panel && state_->module_panel->poll(action));
}
void GameUi::enable_module_actions() {
  state_->module_actions_enabled=true;
  state_->module_actions.clear();
  state_->interact_requested=false;
}

bool GameUi::queue_module_action(const std::string& action) {
  auto& s=*state_;
  if (!s.module_actions_enabled || action.empty()) return false;
  if (s.module_actions.size()>=64) {
    show_notification("Action queue full; try again.");
    std::fprintf(stderr,"module_action_queue=full\n");
    return false;
  }
  s.module_actions.push_back(action);
  return true;
}

bool GameUi::take_module_action(std::string& action) {
  auto& pending=state_->module_actions;
  if (pending.empty()) return false;
  action=std::move(pending.front());
  pending.pop_front();
  return true;
}
}
