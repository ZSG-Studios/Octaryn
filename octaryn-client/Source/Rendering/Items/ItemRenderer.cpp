#include "ItemRenderer.h"
#include "WorldRendererInternal.h"
#include "../../MapWorld/MapDrawBinding.h"
#include "../../MapWorld/MapRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
bool set_item_poses(WorldRenderer& r,std::span<const app::WorldItemPose> poses,std::uint64_t revision) {
  auto& items=r.items;
  if(poses.size()>ItemRenderCapacity)return false;
  if(items.source_revision==revision && items.poses.size()==poses.size())return true;
  for(const auto& pose:poses) {
    if(!pose.entity_id || !pose.generation || !pose.count || !items.asset_lookup.contains(pose.item_id) ||
       !std::isfinite(pose.x) || !std::isfinite(pose.y) || !std::isfinite(pose.z))return false;
  }
  items.poses.assign(poses.begin(),poses.end());items.source_revision=revision;++items.revision;
  std::sort(items.poses.begin(),items.poses.end(),[&](const auto& a,const auto& b) {
    return a.item_id==b.item_id?a.entity_id<b.entity_id:a.item_id<b.item_id;
  });
  return true;
}
bool open_world_renderer_set_items(WorldRenderer* r,std::span<const app::WorldItemPose> poses,std::uint64_t revision) {
  return r && set_item_poses(*r,poses,revision);
}
bool prepare_item_instances(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& items=r.items;items.instances.clear();items.batches.clear();++items.frame;
  const auto now=SDL_GetTicksNS();bool moved=false;
  for(const auto& pose:items.poses) {
    const auto asset=items.asset_lookup.at(pose.item_id);
    if(items.batches.empty() || items.batches.back().asset!=asset)
      items.batches.push_back({asset,unsigned(items.instances.size()),0});
    ++items.batches.back().count;
    const auto found=items.previous.find(pose.entity_id);
    std::array<float,3> position{pose.x,pose.y,pose.z};
    const bool known=found!=items.previous.end() && found->second.generation==pose.generation;
    const auto received=known && found->second.source_tick==pose.source_tick?found->second.received_ns:now;
    if(known && !(pose.flags&2u) && pose.previous_tick<pose.source_tick) {
      const double duration=std::clamp(double(pose.source_tick-pose.previous_tick)/60.0,1.0/60.0,.25);
      const auto alpha=static_cast<float>(std::clamp(double(now-received)/1e9/duration,0.0,1.0));
      const std::array<float,3> source{pose.previous_x,pose.previous_y,pose.previous_z};
      for(unsigned axis=0;axis<3;++axis)position[axis]=std::lerp(source[axis],position[axis],alpha);
    }
    const auto previous=known?
        found->second.position:position;
    moved=moved || position!=previous;
    items.instances.push_back({{position[0],position[1],position[2],0},{previous[0],previous[1],previous[2],0}});
    items.previous.insert_or_assign(pose.entity_id,ItemPreviousPose{pose.generation,items.frame,pose.source_tick,received,position});
  }
  std::erase_if(items.previous,[&](const auto& entry){return entry.second.frame!=items.frame;});
  if(moved)++items.revision;
  if(items.instances.empty())return true;
  return SLANG_SUCCEEDED(commands->uploadBufferData(items.buffers[r.active_frame],0,
      items.instances.size()*sizeof(ItemRenderInstance),items.instances.data()));
}
bool render_items(WorldRenderer& r,rhi::IRenderPassEncoder* pass,bool motion) {
  auto& items=r.items;if(items.instances.empty())return true;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(r.render_width()),float(r.render_height()));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(r.render_width(),r.render_height());
  state.indexFormat=rhi::IndexFormat::Uint32;
  const auto previous=temporal_view(r.temporal.reset?r.temporal.camera:r.temporal.history.previous(),
      r.temporal.reset?r.render_width():r.temporal.history.previous_width(),
      r.temporal.reset?r.render_height():r.temporal.history.previous_height());
  for(const auto& batch:items.batches) {
    auto& map=*items.assets[batch.asset].mesh;
    state.indexBuffer={map.raster_indices.get(),0};pass->setRenderState(state);
    auto* root=pass->bindPipeline(motion?items.motion:items.gbuffer);
    if(!root || !bind_map_geometry(map,root))return false;
    rhi::ShaderCursor cursor(root);
    if(SLANG_FAILED(cursor["itemInstances"].setBinding(rhi::Binding(items.buffers[r.active_frame]))))return false;
    if(cursor["previousItemView"].isValid() && SLANG_FAILED(cursor["previousItemView"].setData(&previous,sizeof(previous))))return false;
    for(const auto& primitive:map.model.primitives) {
      if(!bind_map_draw_uniforms(map,root,primitive,r))return false;
      rhi::DrawArguments draw{};draw.vertexCount=primitive.index_count;draw.startIndexLocation=primitive.first_index;
      draw.instanceCount=batch.count;draw.startInstanceLocation=batch.first;pass->drawIndexed(draw);
    }
  }
  return true;
}
}
