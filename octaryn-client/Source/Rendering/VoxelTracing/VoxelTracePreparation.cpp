#include "VoxelTraceWorld.h"
#include <limits>

namespace octaryn::client::voxel_tracing {
PreparedTraceColumn prepare_trace_column(TraceColumnInput input) {
  PreparedTraceColumn result;result.source=std::move(input.source);result.expected_source=std::move(input.expected_source);
  if(!result.source)return result;
  const auto& column=*result.source;
  if(column.height<=0 || unsigned(column.height)>input.max_column_height ||
      std::uint64_t(column.height)*1024!=column.blocks.size() ||
      std::int64_t(column.min_y)+column.height>std::numeric_limits<std::int32_t>::max())return result;
  const auto first=floor_div(column.min_y,32),last=floor_div(std::int64_t(column.min_y)+column.height-1,32);
  std::map<std::int32_t,std::shared_ptr<const TraceChunk>> retained;
  for(const auto& chunk:input.retained)retained.emplace(chunk->key.y,chunk);
  bool changed=false;
  for(auto y=first;y<=last;++y) {
    const auto old=retained.find(std::int32_t(y));
    auto next=build_trace_chunk(result.source,std::int32_t(y));
    const bool different=old==retained.end() || !equal_trace_content(*old->second,*next);
    if(!different) {
      next->geometry_epoch=old->second->geometry_epoch;
      next->packed_materials=old->second->packed_materials;
    }
    result.chunks.push_back(std::move(next));result.changed.push_back(different);changed|=different;
  }
  changed|=input.retained.size()!=result.chunks.size();
  result.status=changed?PublishStatus::Published:PublishStatus::Unchanged;
  return result;
}
TraceColumnInput VoxelTraceWorld::prepare_input(const world_presentation::StreamColumn& column) const {
  TraceColumnInput input;input.source=std::make_shared<const world_presentation::StreamColumn>(column);
  input.max_column_height=limits_.max_column_height;
  const auto previous=columns_.find({column.x,column.z});
  if(previous!=columns_.end()) {
    input.expected_source=previous->second;
    const auto first=floor_div(previous->second->min_y,32);
    const auto last=floor_div(std::int64_t(previous->second->min_y)+previous->second->height-1,32);
    for(auto y=first;y<=last;++y)input.retained.push_back(find({column.x,std::int32_t(y),column.z}));
  }
  return input;
}
std::size_t VoxelTraceWorld::hierarchy_bytes() const {
  std::size_t bytes=chunks_.size()*sizeof(TraceChunk);
  for(const auto& [key,chunk]:chunks_)if(chunk->packed_materials)bytes+=sizeof(*chunk->packed_materials);
  return bytes;
}
}
