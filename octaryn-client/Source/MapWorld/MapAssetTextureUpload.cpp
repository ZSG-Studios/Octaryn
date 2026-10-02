#include "MapAssetBuildInternal.h"
#include "MapUploadBudget.h"
#include <algorithm>
#include <unordered_set>
#include <chrono>

namespace octaryn::client::rendering {
std::uint64_t map_unique_texture_bytes(std::span<MapRenderer* const> maps) {
  std::uint64_t bytes=0;std::unordered_set<const void*> seen;
  for(const auto* map:maps) {
    if(!map || !seen.insert(map).second)continue;
    if(map->texture_resources.empty()) {bytes+=map->texture_bytes;continue;}
    for(const auto& resource:map->texture_resources)
      if(resource && seen.insert(resource->texture.get()).second)bytes+=resource->bytes;
  }
  return bytes;
}
std::shared_ptr<MapTexturePool> create_map_texture_pool() {return std::make_shared<MapTexturePool>();}
std::shared_ptr<const MapTextureReuseIndex> snapshot_map_texture_reuse(const MapTexturePool& pool,
    const std::filesystem::path& cache) {
  if(cache.empty() || pool.textures.size()>10000)return {};
  if(pool.reuse_snapshot && pool.snapshot_generation==pool.ready_generation &&
      pool.reuse_snapshot->cache_directory==cache)return pool.reuse_snapshot;
  auto index=std::make_shared<MapTextureReuseIndex>();index->cache_directory=cache;
  for(const auto& [key,weak]:pool.textures)if(auto resource=weak.lock()) {
    if(!resource->ready || !resource->validated_content || resource->cache_directory!=cache)continue;
    if(!index->entries.emplace(resource->content_key,MapTextureReuseIndex::Entry{resource,resource->metadata}).second)
      return {}; // Ambiguous cache generations must take the validating miss path.
  }
  pool.snapshot_generation=pool.ready_generation;pool.reuse_snapshot=index;
  return pool.reuse_snapshot;
}
std::uint64_t map_texture_pool_bytes(const MapTexturePool& pool) {
  std::uint64_t bytes=0;for(const auto& [key,weak]:pool.textures)if(const auto texture=weak.lock())bytes+=texture->bytes;
  return bytes;
}
MapTexturePoolStats map_texture_pool_stats(MapTexturePool& pool) {
  MapTexturePoolStats stats;
  unsigned erased=0;
  for(auto entry=pool.textures.begin();entry!=pool.textures.end();) {
    if(const auto texture=entry->second.lock()) {
      stats.allocated+=texture->bytes;(texture->ready?stats.ready:stats.pending)+=texture->bytes;
      ++entry;
    } else if(erased<64) {
      // Reuse the accounting traversal; bound metadata frees per refresh.
      entry=pool.textures.erase(entry);++erased;
    } else ++entry;
  }
  return stats;
}
std::uint64_t map_texture_pool_additional_bytes(const MapTexturePool& pool,const PreparedMapAsset& asset) {
  std::uint64_t bytes=0;
  for(const auto& prepared:asset.images.textures) {
    if(prepared.resident)continue; // The pool still accounts for the lease's strong owner.
    const auto key=map_texture_pool_key(asset.texture_cache,prepared.key,prepared.texture.compressed);
    const auto found=pool.textures.find(key);
    if(found!=pool.textures.end() && !found->second.expired())continue;
    for(const auto& level:prepared.texture.levels)bytes+=level.blocks.size();
  }
  return bytes;
}
bool pump_map_image(MapRendererBuild& build,rhi::ICommandEncoder* commands,std::uint64_t& budget,bool& progressed) {
  progressed=false;auto& map=*build.map;auto& prepared=build.prepared.images.textures[build.image];
  const auto& cached=prepared.texture;
  if(!map.texture_resources[build.image] || !map.textures[build.image])return false;
  if(cached.levels.empty()) {++build.image;progressed=true;return true;}
  auto& level=prepared.texture.levels[build.mip];auto* texture=map.textures[build.image].get();
  rhi::SubresourceLayout layout{};if(SLANG_FAILED(texture->getSubresourceLayout(build.mip,&layout)))return false;
  // 512 conservatively includes the pinned DX12/Vulkan staging-offset alignment.
  const auto align=[](std::uint64_t bytes) {return (bytes+511)&~std::uint64_t(511);};
  const unsigned block_height=cached.compressed?4:1;
  const auto source_pitch=std::uint64_t(cached.compressed?(level.width+3)/4*16:level.width*4);
  const auto remaining=(level.height-build.row+block_height-1)/block_height;
  const auto rows=map_texture_upload_rows(remaining,layout.rowPitch,budget);
  if(!rows)return true;
  const unsigned height=std::min<unsigned>(static_cast<unsigned>(rows)*block_height,level.height-build.row);
  rhi::SubresourceRange range{};range.layerCount=1;range.mip=build.mip;range.mipCount=1;
  rhi::SubresourceData data{level.blocks.data()+std::uint64_t(build.row/block_height)*source_pitch,
      source_pitch,source_pitch*rows};
  const auto start=std::chrono::steady_clock::now();
  if(SLANG_FAILED(commands->uploadTextureData(texture,range,{0,build.row,0},{level.width,height,1},&data,1)))return false;
  build.progress.max_upload_call_ms=std::max(build.progress.max_upload_call_ms,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  build.progress.commands_recorded=true;
  budget-=align(rows*layout.rowPitch);build.row+=height;progressed=true;
  if(build.row==level.height) {
    std::vector<std::uint8_t>().swap(level.blocks);build.row=0;++build.mip;
    if(build.mip==cached.levels.size()) {
      commands->setTextureState(texture,rhi::ResourceState::ShaderResource);
      prepared.texture.levels.clear();build.mip=0;++build.image;
    }
  }
  return true;
}
}
