#include "GameUiState.h"

namespace octaryn::client::app {
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
