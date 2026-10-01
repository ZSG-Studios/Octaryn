#pragma once
#include "MapAssetPrepare.h"
#include <slang-rhi.h>
#include <array>
#include <memory>
#include <atomic>

namespace octaryn::client::rendering {
struct MapRenderer;
struct MapRendererBuild;
struct MapTexturePool;
// The pool holds weak records; resident maps retain the textures they reference.
std::shared_ptr<MapTexturePool> create_map_texture_pool();
std::shared_ptr<const MapTextureReuseIndex> snapshot_map_texture_reuse(const MapTexturePool&,const std::filesystem::path&);
// Loading-screen operation: compiles shared pipelines before streaming frame work.
bool prewarm_map_pipeline_pool(MapTexturePool&,rhi::IDevice*,rhi::Format color,rhi::Format depth);
std::uint64_t map_texture_pool_bytes(const MapTexturePool&);
struct MapTexturePoolStats {std::uint64_t allocated{},ready{},pending{};};
MapTexturePoolStats map_texture_pool_stats(MapTexturePool&);
std::uint64_t map_texture_pool_additional_bytes(const MapTexturePool&,const PreparedMapAsset&);
std::uint64_t map_prepared_geometry_bytes(const PreparedMapAsset&);
MapRendererBuild* begin_map_renderer_build(rhi::IDevice*,rhi::Format color,rhi::Format depth,
    PreparedMapAsset&&,std::shared_ptr<MapTexturePool>,bool ray_required=false);
enum class MapBuildStatus {Progress,NeedsSubmission,Waiting,Ready,Failed};
struct MapBuildProgress {
  std::uint64_t uploaded_this_pump{},total_uploaded{},pending_bytes{},gpu_allocated{};
  double cpu_ms{};
  double allocation_ms{},allocation_max_call_ms{},material_ms{},max_upload_call_ms{};
  unsigned allocated_resources{},material_records{};
  bool allocation_pending{};
  bool commands_recorded{}; // Writes to the caller's encoder in this pump.
};
MapBuildProgress map_renderer_build_progress(const MapRendererBuild*);
MapBuildStatus pump_map_renderer_build(MapRendererBuild*,rhi::ICommandEncoder*,std::uint64_t byte_budget,double milliseconds);
// Call only after the command buffer containing the final uploads is submitted.
void map_renderer_build_submitted(MapRendererBuild*,rhi::IFence*,std::uint64_t value);
MapRenderer* take_map_renderer_build(MapRendererBuild*);
void destroy_map_renderer_build(MapRendererBuild*);
void cancel_map_renderer_build(MapRendererBuild*);
}
