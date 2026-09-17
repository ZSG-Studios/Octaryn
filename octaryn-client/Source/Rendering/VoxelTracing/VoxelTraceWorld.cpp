#include "VoxelTraceWorld.h"
#include <algorithm>
#include <limits>

namespace octaryn::client::voxel_tracing {
VoxelTraceWorld::VoxelTraceWorld(TraceLimits limits):limits_(limits) {
  limits_.max_changes=std::max(std::size_t(1),limits_.max_changes);
}
void VoxelTraceWorld::changed(ChunkKey key,ChangeKind kind) {
  changes_.push_back({key,++epoch_,kind});
  if(changes_.size()>limits_.max_changes)changes_.pop_front();
}
PublishResult VoxelTraceWorld::publish(const world_presentation::StreamColumn& column) {
  return publish_prepared(prepare_trace_column(prepare_input(column)));
}
PublishResult VoxelTraceWorld::publish_prepared(PreparedTraceColumn prepared) {
  if((prepared.status!=PublishStatus::Published && prepared.status!=PublishStatus::Unchanged) || !prepared.source ||
      prepared.chunks.size()!=prepared.changed.size())return {};
  const auto& column=*prepared.source;
  const ColumnKey coordinate{column.x,column.z};
  const auto previous=columns_.find(coordinate);
  if((previous==columns_.end()?nullptr:previous->second)!=prepared.expected_source)return {PublishStatus::Stale,0};
  const auto first=std::int32_t(floor_div(column.min_y,32));
  const auto last=std::int32_t(floor_div(std::int64_t(column.min_y)+column.height-1,32));
  std::size_t old_count=0;
  if(previous!=columns_.end())old_count=std::size_t(floor_div(std::int64_t(previous->second->min_y)+previous->second->height-1,32)-
      floor_div(previous->second->min_y,32)+1);
  if(chunks_.size()-old_count+std::size_t(std::int64_t(last)-first+1)>limits_.max_chunks)
    return {PublishStatus::Capacity,0};
  if(prepared.chunks.size()!=std::size_t(std::int64_t(last)-first+1))return {};
  for(std::size_t i=0;i<prepared.chunks.size();++i)if(!prepared.chunks[i] ||
      prepared.chunks[i]->key!=ChunkKey{column.x,first+std::int32_t(i),column.z} ||
      prepared.chunks[i]->source!=prepared.source)return {};
  unsigned removed=0;
  if(previous!=columns_.end()) {
    const auto old_first=floor_div(previous->second->min_y,32);
    const auto old_last=floor_div(std::int64_t(previous->second->min_y)+previous->second->height-1,32);
    for(auto y=old_first;y<=old_last;++y)if(y<first || y>last) {
      const ChunkKey key{column.x,std::int32_t(y),column.z};
      chunks_.erase(key);changed(key,ChangeKind::Removed);++removed;
    }
  }
  unsigned count=removed;
  for(std::size_t i=0;i<prepared.chunks.size();++i) {
    auto& chunk=prepared.chunks[i];
    if(prepared.changed[i]) {changed(chunk->key,ChangeKind::Published);chunk->geometry_epoch=epoch_;++count;}
    chunks_.insert_or_assign(chunk->key,std::move(chunk));
  }
  // Current chunks share one current compact column, rather than pinning one
  // complete historical column per independently edited vertical chunk.
  columns_.insert_or_assign(coordinate,std::move(prepared.source));
  return {count?PublishStatus::Published:PublishStatus::Unchanged,count};
}
unsigned VoxelTraceWorld::remove(std::int32_t x,std::int32_t z) {
  const auto found=columns_.find({x,z});if(found==columns_.end())return 0;
  const auto first=floor_div(found->second->min_y,32);
  const auto last=floor_div(std::int64_t(found->second->min_y)+found->second->height-1,32);
  unsigned count=0;
  for(auto y=first;y<=last;++y) {
    const ChunkKey key{x,std::int32_t(y),z};
    chunks_.erase(key);changed(key,ChangeKind::Removed);++count;
  }
  columns_.erase(found);return count;
}
std::shared_ptr<const TraceChunk> VoxelTraceWorld::find(ChunkKey key) const {
  const auto found=chunks_.find(key);return found==chunks_.end()?nullptr:found->second;
}
VoxelSample VoxelTraceWorld::sample(std::int64_t x,std::int64_t y,std::int64_t z) const {
  const std::array<std::int64_t,3> cell{floor_div(x,32),floor_div(y,32),floor_div(z,32)};
  for(auto v:cell)if(v<std::numeric_limits<std::int32_t>::min() || v>std::numeric_limits<std::int32_t>::max())return {};
  const auto chunk=find({std::int32_t(cell[0]),std::int32_t(cell[1]),std::int32_t(cell[2])});
  if(!chunk)return {};
  return chunk->sample(unsigned(x-cell[0]*32),unsigned(y-cell[1]*32),unsigned(z-cell[2]*32));
}
bool VoxelTraceWorld::changes_since(std::uint64_t cursor,std::vector<TraceChange>& result) const {
  result.clear();if(cursor>epoch_)return false;
  if(!changes_.empty() && cursor<changes_.front().geometry_epoch-1)return false;
  for(const auto& change:changes_)if(change.geometry_epoch>cursor)result.push_back(change);
  return true;
}
}
