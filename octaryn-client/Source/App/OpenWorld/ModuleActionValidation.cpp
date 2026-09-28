#include "ModuleActionValidation.h"
#include "GameUi.h"
#include <cstdio>

namespace octaryn::client::app {
void ModuleActionValidation::start(GameUi& ui) {
  if (!enabled_ || started_) return;
  started_=true;
  // Exercise the same UI queue as keyboard commands, without OS input.
  for (const char* action:{"inventory.select.1","inventory.select.0","inventory.drop","inventory.drop"})
    if (!ui.queue_module_action(action)) failed_=true;
  std::puts("module_action_validation started=1 burst=4 expected_drops=2");
}
void ModuleActionValidation::event(std::uint64_t id,std::uint64_t kind,std::uint64_t item,std::uint64_t count) {
  if (!enabled_ || !started_ || kind!=2) return;
  if (!item_) item_=item;
  if (!receipts_.insert(id).second || !item || item!=item_ || count!=1) failed_=true;
  ++drops_;
}
bool ModuleActionValidation::finish() const {
  if (!enabled_) return true;
  const bool valid=started_ && !failed_ && drops_==2;
  std::printf("module_action_validation=%s drops=%u unique_receipts=%zu\n",
      valid?"passed":"failed",drops_,receipts_.size());
  return valid;
}
}
