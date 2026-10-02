#include "MapAssetBuildInternal.h"
#include <cstdlib>
#include <cstring>

namespace octaryn::client::rendering {
bool prewarm_map_pipeline_pool(MapTexturePool& pool,rhi::IDevice* device,rhi::Format color,rhi::Format depth) {
  if(!device || (pool.device && pool.device.get()!=device))return false;
  pool.device=device;
  const auto key=std::make_tuple(unsigned(color),unsigned(depth),0u);
  if(pool.pipelines.contains(key))return true;
  auto prototype=std::make_shared<MapRenderer>();prototype->device=device;
  if(!create_map_pipelines(*prototype,color,depth,"octaryn-client/Shaders/Map/WorldMap.slang"))return false;
  pool.pipelines.emplace(key,std::move(prototype));return true;
}
bool bind_map_cached_pipelines(MapTexturePool& pool,MapRenderer& map,rhi::Format color,rhi::Format depth) {
  if(pool.device.get()!=map.device.get())return false;
  const auto found=pool.pipelines.find({unsigned(color),unsigned(depth),0u});
  if(found==pool.pipelines.end())return false;
  const auto& prototype=*found->second;
  map.forward_pipeline=prototype.forward_pipeline;
  map.forward_rt_pipeline=prototype.forward_rt_pipeline;
  map.additive_pipeline=prototype.additive_pipeline;map.additive_rt_pipeline=prototype.additive_rt_pipeline;return true;
}
}
