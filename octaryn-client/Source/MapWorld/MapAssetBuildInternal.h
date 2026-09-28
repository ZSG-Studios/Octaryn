#pragma once
#include "MapAssetBuild.h"
#include "MapRendererInternal.h"
#include "MapIndirectData.h"
#include "MapSamplerCache.h"
#include <map>
#include <tuple>
#include <future>

namespace octaryn::client::rendering {
struct MapTextureResource {
  Slang::ComPtr<rhi::ITexture> texture;
  Slang::ComPtr<rhi::ITextureView> view;
  std::uint64_t bytes{};
  bool ready{};
  bool validated_cache{};
  std::filesystem::path cache_directory;
  std::string content_key;
  MapTextureReuseMetadata metadata;
};
struct MapTexturePool {
  std::shared_ptr<MapSamplerCache> samplers=std::make_shared<MapSamplerCache>();
  Slang::ComPtr<rhi::IDevice> device;
  std::map<std::string,std::weak_ptr<MapTextureResource>> textures;
  std::uint64_t ready_generation{};
  mutable std::uint64_t snapshot_generation{UINT64_MAX};
  mutable std::shared_ptr<const MapTextureReuseIndex> reuse_snapshot;
  std::map<std::tuple<unsigned,unsigned,unsigned>,std::shared_ptr<MapRenderer>> pipelines;
};
struct MapRendererBuild {
  std::unique_ptr<MapRenderer> map;
  PreparedMapAsset prepared;
  std::shared_ptr<MapTexturePool> pool;
  std::vector<std::shared_ptr<MapTextureResource>> created;
  rhi::Format color{},depth{};
  unsigned stage{},geometry{},mip{},row{};
  unsigned meshlet_buffer{};
  size_t image{};
  std::uint64_t offset{},submitted_value{};
  std::vector<MapRayMaterial> materials;
  std::vector<MapIndirectPrimitive> indirect;
  MapBuildProgress progress;
  std::atomic_bool cancelled{};
  std::future<bool> allocation;
  bool allocation_ready{};
  bool allocate_ray{};
  MapBuildProgress allocation_progress;
  Slang::ComPtr<rhi::IFence> submitted_fence;
  MapBuildStatus status{MapBuildStatus::Progress};
};
bool prepare_map_materials(MapRenderer&,std::vector<MapRayMaterial>&,size_t maximum=SIZE_MAX);
bool start_map_resource_allocation(MapRendererBuild&);
bool create_map_pipelines(MapRenderer&,rhi::Format,rhi::Format,const char* shader_path);
bool bind_map_cached_pipelines(MapTexturePool&,MapRenderer&,rhi::Format,rhi::Format);
bool pump_map_image(MapRendererBuild&,rhi::ICommandEncoder*,std::uint64_t& budget,bool& progressed);
}
