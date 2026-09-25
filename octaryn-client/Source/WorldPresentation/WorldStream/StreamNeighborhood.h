#pragma once
#include "StreamSnapshot.h"
#include <cstdlib>
#include <map>

namespace octaryn::client::world_presentation {
// Worker-private identities from one authoritative snapshot and requested window.
class StreamNeighborhood {
public:
  void reset(const StreamSnapshot& snapshot,int x,int z,unsigned radius) {
    columns_.clear();
    for(const auto& column:snapshot.columns)
      if(std::abs(std::int64_t(column.x)-x)<=radius &&
          std::abs(std::int64_t(column.z)-z)<=radius)
        columns_.emplace(std::make_pair(column.x,column.z),&column);
  }
  std::array<std::optional<SnapshotColumn>,8> capture(const SnapshotColumn& center) const {
    std::array<std::optional<SnapshotColumn>,8> result;
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
      if(!dx && !dz)continue;
      const auto found=columns_.find({center.x+dx,center.z+dz});
      if(found!=columns_.end())result[column_neighbor_index(dx,dz)]=*found->second;
    }
    return result;
  }
private:
  std::map<std::pair<int,int>,const SnapshotColumn*> columns_;
};
}
