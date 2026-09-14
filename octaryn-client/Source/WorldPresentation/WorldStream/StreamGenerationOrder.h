#pragma once
#include "StreamSnapshot.h"
#include "StreamResidency.h"
#include <tuple>

namespace octaryn::client::world_presentation {
// Worker-owned pointers remain valid until the next successful snapshot read.
class StreamGenerationOrder {
public:
  void reset(const StreamSnapshot& snapshot,int x,int z,unsigned radius) {
    columns_.clear();cursor_=0;
    for(const auto& column:snapshot.columns)
      if(std::abs(static_cast<std::int64_t>(column.x)-x)<=radius &&
          std::abs(static_cast<std::int64_t>(column.z)-z)<=radius) columns_.push_back(&column);
    const auto priority=[=](const SnapshotColumn* column) {
      const auto dx=std::abs(static_cast<std::int64_t>(column->x)-x);
      const auto dz=std::abs(static_cast<std::int64_t>(column->z)-z);
      return std::tuple{std::max(dx,dz),dx+dz};
    };
    std::stable_sort(columns_.begin(),columns_.end(),[&](const auto* a,const auto* b) {
      return priority(a)<priority(b);
    });
  }
  const SnapshotColumn* next(const StreamResidency& state) {
    while(cursor_<columns_.size()) {
      const auto* column=columns_[cursor_];
      if(state.needs_generation(column->x,column->z,column->revision)) return column;
      ++cursor_;
    }
    return nullptr;
  }
  void consumed() {++cursor_;}
private:
  std::vector<const SnapshotColumn*> columns_;
  std::size_t cursor_{};
};
}
