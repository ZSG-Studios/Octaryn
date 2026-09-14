#pragma once
#include "WorldStream.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <map>

namespace octaryn::client::world_presentation {
// Internal CPU residency owner. WorldStream holds its mutex for every operation;
// generation may run outside that mutex and must recheck the current window.
struct StreamResidency {
  struct ReadyColumn {
    StreamColumn column;
    std::shared_ptr<const StreamColumn> query;
  };
  struct QueryColumn {
    std::shared_ptr<const StreamColumn> column;
    bool visible{};
  };
  using QueryMap=std::map<std::pair<int,int>,QueryColumn>;
  struct RetiredPayloads {
    std::array<std::shared_ptr<const StreamColumn>,2> queries;
    std::vector<ReadyColumn> ready;
    std::vector<QueryMap::node_type> columns;
    RetiredPayloads() {ready.reserve(2);columns.reserve(65*65+2);}
    bool empty() const {return !queries[0] && !queries[1] && ready.empty() && columns.empty();}
    // The worker calls this without the residency mutex held.
    void clear() {for(auto& query:queries) query.reset();ready.clear();columns.clear();}
  };
  std::deque<ReadyColumn> ready;
  QueryMap query_columns;
  std::array<std::shared_ptr<const StreamColumn>,2> retired_queries;
  std::map<std::pair<int,int>,std::uint64_t> completed;
  std::int32_t x{},z{};
  std::uint32_t radius{2};
  std::uint64_t window_revision{};
  bool spatial_prune_pending{true};

  bool wanted(int cx,int cz) const {
    return std::abs(static_cast<std::int64_t>(cx)-x)<=radius &&
           std::abs(static_cast<std::int64_t>(cz)-z)<=radius;
  }
  bool change_window(std::int32_t next_x,std::int32_t next_z,std::uint32_t next_radius) {
    next_x=std::clamp(next_x,-1000000,1000000);
    next_z=std::clamp(next_z,-1000000,1000000);
    next_radius=std::clamp(next_radius,1u,32u);
    if(x==next_x && z==next_z && radius==next_radius) return false;
    x=next_x;z=next_z;radius=next_radius;
    ++window_revision;
    spatial_prune_pending=true;
    // Observe every requested window, even if the worker is still generating.
    // A later reversal must not mistake GPU-retired columns for completed work.
    std::erase_if(completed,[this](const auto& entry) {return !wanted(entry.first.first,entry.first.second);});
    for(auto& [coordinate,query]:query_columns)
      if(!wanted(coordinate.first,coordinate.second)) query.visible=false;
    return true;
  }
  void collect_retired(RetiredPayloads& retired) {
    // Destination is empty with capacity for the bounded window/mailbox. Move
    // ownership only under the mutex; payload and map-node frees happen outside.
    assert(retired.empty());
    retired.queries.swap(retired_queries);
    if(!spatial_prune_pending) return;
    spatial_prune_pending=false;
    for(auto it=ready.begin();it!=ready.end();) {
      if(wanted(it->column.x,it->column.z)) {++it;continue;}
      retired.ready.push_back(std::move(*it));
      it=ready.erase(it);
    }
    for(auto it=query_columns.begin();it!=query_columns.end();) {
      if(wanted(it->first.first,it->first.second)) {++it;continue;}
      retired.columns.push_back(query_columns.extract(it++));
    }
  }
  bool needs_generation(int cx,int cz,std::uint64_t revision) const {
    if(!wanted(cx,cz)) return false;
    const auto found=completed.find({cx,cz});
    return found==completed.end() || found->second!=revision;
  }
  bool retain(StreamColumn column,std::shared_ptr<const StreamColumn> query) {
    if(!wanted(column.x,column.z) || ready.size()>=2) return false;
    const auto coordinate=std::make_pair(column.x,column.z);
    const auto revision=column.revision;
    const auto [completion,inserted]=completed.try_emplace(coordinate,0);
    // Mark complete only after publishing to the bounded delivery queue succeeds.
    try {ready.push_back({std::move(column),std::move(query)});}
    catch(...) {if(inserted) completed.erase(completion);throw;}
    completion->second=revision;
    return true;
  }
  static bool same_payload(const StreamColumn& a,const StreamColumn& b) {
    return a.x==b.x && a.z==b.z && a.epoch==b.epoch && a.revision==b.revision &&
        a.min_y==b.min_y && a.height==b.height && a.blocks.storage_identity()==b.blocks.storage_identity();
  }
  auto next_ready(const StreamColumn* excluded=nullptr) {
    return std::find_if(ready.begin(),ready.end(),[this,excluded](const auto& value) {
      return wanted(value.column.x,value.column.z) && (!excluded || !same_payload(value.column,*excluded));
    });
  }
  bool peek(StreamColumn& output,const StreamColumn* excluded=nullptr) {
    const auto entry=next_ready(excluded);
    if(entry==ready.end())return false;
    output=entry->column;return true;
  }
  StreamPublication publish(const StreamColumn& expected) {
    if(!wanted(expected.x,expected.z))return StreamPublication::Retired;
    const auto entry=std::find_if(ready.begin(),ready.end(),[&](const auto& value) {
      return same_payload(value.column,expected);
    });
    if(entry==ready.end())return StreamPublication::Retired;
    StreamColumn delivered;
    return deliver_ready(entry,delivered)?StreamPublication::Published:StreamPublication::Busy;
  }
  bool deliver(StreamColumn& output) {return deliver_ready(next_ready(),output);}
  bool deliver_ready(std::deque<ReadyColumn>::iterator entry,StreamColumn& output) {
    if(entry==ready.end()) return false;
    const auto coordinate=std::make_pair(entry->column.x,entry->column.z);
    const auto old=query_columns.find(coordinate);
    auto retired=retired_queries.end();
    if(old!=query_columns.end() && old->second.column) {
      retired=std::find_if(retired_queries.begin(),retired_queries.end(),[](const auto& value) {return !value;});
      if(retired==retired_queries.end()) return false; // Worker cleanup provides bounded backpressure.
    }
    // Complete any small map allocation before moving ownership. The exact queued
    // query travels with its payload; a newer worker completion cannot replace it.
    auto& destination=query_columns.try_emplace(coordinate).first->second;
    if(retired!=retired_queries.end()) *retired=std::move(destination.column);
    destination.column=std::move(entry->query);
    destination.visible=true;
    output=std::move(entry->column);
    ready.erase(entry); // Moved-from payload only; heavy destruction stays on worker.
    return true;
  }
  std::shared_ptr<const StreamColumn> query(int cx,int cz) const {
    if(!wanted(cx,cz)) return {};
    const auto found=query_columns.find({cx,cz});
    return found!=query_columns.end() && found->second.visible?found->second.column:nullptr;
  }
};
}
