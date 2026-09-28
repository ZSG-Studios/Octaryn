#pragma once
#include "../../MapWorld/MapRenderer.h"
#include "../../App/LocalSession/WorldItemPose.h"
#include "ItemHistory.h"
#include "ItemHistoryMemory.h"
#include <unordered_map>

namespace octaryn::client::rendering {
inline constexpr unsigned ItemRenderCapacity=10000;
struct ItemRenderAsset {std::uint32_t item_id{};std::shared_ptr<MapRenderer> mesh;};
struct ItemRenderInstance {std::array<float,4> current{},previous{};};
static_assert(sizeof(ItemRenderInstance)==32);
struct ItemRenderBatch {unsigned asset{},first{},count{};};
struct ItemRenderer {
  std::vector<ItemRenderAsset> assets;
  std::unordered_map<std::uint32_t,unsigned> asset_lookup;
  ItemHistoryMemory previous_memory;
  ItemHistory previous{&previous_memory};
  std::vector<app::WorldItemPose> poses;
  std::vector<ItemRenderInstance> instances;
  std::vector<ItemRenderBatch> batches;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> buffers;
  Slang::ComPtr<rhi::IRenderPipeline> gbuffer,motion;
  std::uint64_t revision{},source_revision{},frame{},gpu_bytes{};
  bool initialized{};
};
bool initialize_item_renderer(WorldRenderer&);
bool prepare_item_instances(WorldRenderer&,rhi::ICommandEncoder*);
bool render_items(WorldRenderer&,rhi::IRenderPassEncoder*,bool motion);
bool set_item_poses(WorldRenderer&,std::span<const app::WorldItemPose>,std::uint64_t revision);
}
