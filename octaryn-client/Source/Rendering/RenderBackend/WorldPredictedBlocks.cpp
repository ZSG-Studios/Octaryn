#include "WorldRendererInternal.h"
#include "PredictedColumn.h"

namespace octaryn::client::rendering {
namespace {
using world_presentation::PredictedBlocks;
using Coordinate=std::pair<std::int32_t,std::int32_t>;
void compose(WorldRenderer& r,const Coordinate& coordinate) {
  const auto base=r.prediction_bases.find(coordinate),source=r.sources.find(coordinate);
  if(base==r.prediction_bases.end() || source==r.sources.end())return;
  auto composed=world_presentation::compose_predicted_column(base->second,r.predicted_edits);
  // Invalidation compares the old presentation source against the replacement.
  world_mesh_invalidate_neighbors(r,composed);
  source->second=std::move(composed);
  world_block_lights_store(r,source->second);
  r.dirty.insert(coordinate);r.dirty_urgent.insert(coordinate);
  if(!r.predicted_edits.contains_column(coordinate.first,coordinate.second))r.prediction_bases.erase(base);
}
}
bool open_world_renderer_can_predict(const WorldRenderer* r) {
  return r && r->predicted_edits.can_submit();
}
bool world_renderer_same_authoritative_content(const WorldRenderer& r,const world_presentation::StreamColumn& column) {
  const Coordinate coordinate{column.x,column.z};
  const auto source=r.sources.find(coordinate);
  if(source==r.sources.end() || !r.columns.contains(coordinate))return false;
  const auto base=r.prediction_bases.find(coordinate);
  const auto& retained=base==r.prediction_bases.end()?source->second:base->second;
  return retained.revision==column.revision && retained.blocks.storage_identity()==column.blocks.storage_identity();
}
void world_renderer_publish_column_metadata(WorldRenderer& r,const world_presentation::StreamColumn& column) {
  const Coordinate coordinate{column.x,column.z};
  auto& source=r.sources.at(coordinate);
  source.authoritative_revision=column.authoritative_revision;source.epoch=column.epoch;
  const auto base=r.prediction_bases.find(coordinate);
  if(base!=r.prediction_bases.end())base->second=column;
  const auto before=r.predicted_edits.edits().size();
  r.predicted_edits.cover(column.x,column.z,column.authoritative_revision);
  if(before!=r.predicted_edits.edits().size())compose(r,coordinate);
}
bool open_world_renderer_apply_predicted_edit(WorldRenderer* r,std::uint64_t command,
    std::int32_t x,std::int32_t y,std::int32_t z,std::uint16_t block) {
  if(!r)return false;
  const Coordinate coordinate{PredictedBlocks::column(x),PredictedBlocks::column(z)};
  const auto source=r->sources.find(coordinate);
  std::size_t index{};
  if(source==r->sources.end() || !world_presentation::predicted_block_index(source->second,x,y,z,index))return false;
  if(!r->predicted_edits.add(command,x,y,z,block))return false;
  r->prediction_bases.try_emplace(coordinate,source->second);
  compose(*r,coordinate);
  return true;
}
void world_renderer_reapply_predicted_edits(WorldRenderer& r,const Coordinate& coordinate) {
  const auto source=r.sources.find(coordinate);
  if(source==r.sources.end())return;
  const bool had_overlay=r.prediction_bases.contains(coordinate);
  r.predicted_edits.cover(coordinate.first,coordinate.second,source->second.authoritative_revision);
  if(had_overlay || r.predicted_edits.contains_column(coordinate.first,coordinate.second)) {
    r.prediction_bases.insert_or_assign(coordinate,source->second);
    compose(r,coordinate);
  }
}
void open_world_renderer_resolve_predicted_edit(WorldRenderer* r,std::uint64_t command,bool accepted,std::uint64_t revision) {
  if(!r)return;
  const auto edits=r->predicted_edits.edits();
  for(const auto& edit:edits)if(edit.command==command) {
    const Coordinate coordinate{PredictedBlocks::column(edit.x),PredictedBlocks::column(edit.z)};
    r->predicted_edits.resolve(command,accepted,revision);
    const auto base=r->prediction_bases.find(coordinate);
    if(base!=r->prediction_bases.end())r->predicted_edits.cover(coordinate.first,coordinate.second,base->second.authoritative_revision);
    compose(*r,coordinate);
    return;
  }
}
void open_world_renderer_reset_predictions(WorldRenderer* r) {
  if(!r)return;
  r->predicted_edits.clear();
  std::vector<Coordinate> columns;
  for(const auto& [coordinate,base]:r->prediction_bases)columns.push_back(coordinate);
  for(const auto& coordinate:columns)compose(*r,coordinate);
  r->prediction_bases.clear();
}
}
