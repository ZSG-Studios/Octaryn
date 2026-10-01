#pragma once
#include "ItemHistory.h"
#include "ItemHistoryMemory.h"

namespace octaryn::client::rendering {
// Keep the table and its retained allocator together when its owner is committed.
struct ItemHistoryState {
  ItemHistoryMemory memory;
  ItemHistory poses;
  explicit ItemHistoryState(std::pmr::memory_resource* upstream=std::pmr::new_delete_resource())
      :memory(upstream),poses(&memory) {}
};
}
