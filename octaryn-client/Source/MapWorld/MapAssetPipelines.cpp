#include "MapAssetBuildInternal.h"
#include <cstdlib>
#include <cstring>

namespace octaryn::client::rendering {
bool prewarm_map_pipeline_pool(MapTexturePool& pool,rhi::IDevice* device,rhi::Format color,rhi::Format depth) {
  if(!device || (pool.device && pool.device.get()!=device))return false;
  pool.device=device;
  const auto* mode=std::getenv("OCTARYN_CLIENT_MAP_DRAW_MODE");
  const bool indirect=mode && std::strcmp(mode,"indirect")==0;
  const bool meshlet=map_meshlet_requested();
  const auto key=std::make_tuple(unsigned(color),unsigned(depth),meshlet?2u:indirect?1u:0u);
  if(pool.pipelines.contains(key))return true;
  auto prototype=std::make_shared<MapRenderer>();prototype->device=device;prototype->indirect_enabled=indirect;
  prototype->meshlet_enabled=meshlet;
  if(!create_map_pipelines(*prototype,color,depth,"octaryn-client/Shaders/Map/WorldMap.slang"))return false;
  pool.pipelines.emplace(key,std::move(prototype));return true;
}
bool bind_map_cached_pipelines(MapTexturePool& pool,MapRenderer& map,rhi::Format color,rhi::Format depth) {
  if(pool.device.get()!=map.device.get())return false;
  const auto found=pool.pipelines.find({unsigned(color),unsigned(depth),map.meshlet_enabled?2u:map.indirect_enabled?1u:0u});
  if(found==pool.pipelines.end())return false;
  const auto& prototype=*found->second;
  map.gbuffer_pipeline=prototype.gbuffer_pipeline;map.indirect_gbuffer_pipeline=prototype.indirect_gbuffer_pipeline;
  map.meshlet_pipeline=prototype.meshlet_pipeline;
  map.forward_pipeline=prototype.forward_pipeline;
  map.forward_rt_pipeline=prototype.forward_rt_pipeline;map.shadow_pipeline=prototype.shadow_pipeline;
  map.local_shadow_pipeline=prototype.local_shadow_pipeline;return true;
}
}
