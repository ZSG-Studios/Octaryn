#pragma once
#include <slang-rhi.h>
#include "MapModel.h"
#include "MapRayWork.h"
#include <array>
#include <cstddef>
#include <span>
#include <memory>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct MapRenderer;
struct MapRaySubmitScope;
struct MapForwardDraw {MapRenderer* map{};std::uint32_t primitive{};float distance{};std::uint32_t order{},instance{UINT32_MAX};};
using MapLoadProgressFn = void (*)(const char* stage,bool cpu_only,void* user);
// Loads a bounded map, prepares VG pages and uploads textures plus BLEND geometry.
// Returns null with a diagnostic when source preparation fails.
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path,bool world_geometry=true,
    MapLoadProgressFn progress=nullptr,void* progress_user=nullptr);
void destroy_map_renderer(MapRenderer*);
const MapModel& map_model(const MapRenderer&);
// Call only after collision consumers have copied geometry and uploads complete.
void release_map_cpu_geometry(MapRenderer*);
struct MapMemoryStats {
  std::uint64_t textures{},geometry{},acceleration{},scratch{},reserved{};
};
MapMemoryStats map_memory_stats(const MapRenderer&);
std::uint64_t map_unique_texture_bytes(std::span<MapRenderer* const>);
// Blended map primitives are sorted globally and retain their forward material path.
bool render_maps_forward(std::span<const std::shared_ptr<MapRenderer>>,rhi::IRenderPassEncoder*,
    const WorldCamera&,WorldRenderer&);
// Complete static RT loading before publishing map readiness; releases scratch.
bool initialize_map_ray_scene(MapRenderer&,rhi::ICommandQueue*);
// Owner-thread deferred enable: submit once, publish only after fence completion.
bool pump_map_ray_scene(MapRenderer&,rhi::ICommandQueue*,bool requested,const MapRaySubmitScope* profile=nullptr);
// Polling only observes fences/futures and releases completed ownership. It never
// records, submits, allocates, launches a worker, or waits for completion.
MapRayStep poll_map_ray_scene(MapRenderer&);
bool perform_map_ray_work(MapRenderer&,rhi::ICommandQueue*,MapRayStep,const MapRaySubmitScope* profile=nullptr);
void finish_map_ray_scene(MapRenderer&);
// Records a build without publishing readiness. Caller owns submission/fence.
bool prepare_map_ray_scene(MapRenderer&,rhi::ICommandEncoder*);
bool map_ray_ready(const MapRenderer&);
bool map_ray_retirement_ready(const MapRenderer&);
rhi::IAccelerationStructure* map_ray_blas(const MapRenderer&);
// Tolerant ray-query buffer bindings for passes that include WorldRayQuery.
struct MapRayGeometry {
  std::uint64_t vertices{},indices{},materials{},triangles{};
  // Clustered bit 0 selects triangle records; bit 1 applies a scene-node transform.
  std::uint32_t first_triangle{},clustered{};
  std::uint32_t transform_padding[2]{};
  std::array<float,12> world{1,0,0,0,0,1,0,0,0,0,1,0};
  std::array<float,12> normal{1,0,0,0,0,1,0,0,0,0,1,0};
  float orientation{1};std::uint32_t vertex_stride{80};
  std::uint32_t tail_padding[2]{};
};
static_assert(sizeof(MapRayGeometry)==160 && offsetof(MapRayGeometry,world)==48 &&
    offsetof(MapRayGeometry,normal)==96 && offsetof(MapRayGeometry,orientation)==144 &&
    offsetof(MapRayGeometry,vertex_stride)==148);
bool map_ray_geometry(const MapRenderer&,MapRayGeometry&);
bool bind_map_ray_buffers(MapRenderer&,rhi::IShaderObject* root);
}
