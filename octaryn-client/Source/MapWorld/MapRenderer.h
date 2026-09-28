#pragma once
#include <slang-rhi.h>
#include "MapModel.h"
#include "MapRayWork.h"
#include <array>
#include <span>
#include <memory>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct MapRenderer;
struct MapRaySubmitScope;
struct MapForwardDraw {MapRenderer* map{};std::uint32_t primitive{};float distance{};std::uint32_t order{};};
// Loads the flattened GLB scene, uploads buffers/textures and compiles the
// G-buffer plus forward pipelines. Returns null (with a stderr diagnostic) on
// any malformed asset so callers can refuse the world cleanly.
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path);
void destroy_map_renderer(MapRenderer*);
const MapModel& map_model(const MapRenderer&);
// Call only after collision consumers have copied geometry and uploads complete.
void release_map_cpu_geometry(MapRenderer*);
struct MapMemoryStats {
  std::uint64_t textures{},geometry{},acceleration{},scratch{};
};
MapMemoryStats map_memory_stats(const MapRenderer&);
std::uint64_t map_unique_texture_bytes(std::span<MapRenderer* const>);
bool prepare_map_draws(MapRenderer*,rhi::ICommandEncoder*,const WorldCamera&,WorldRenderer&,unsigned phase=0);
// True when the map participates in two-phase Hi-Z occlusion (indirect mode
// with OCTARYN_CLIENT_MAP_OCCLUSION enabled).
bool map_occlusion_active(const MapRenderer*);
// Opaque draws write the five G-buffer targets; forward draws blended
// primitives far to near with depth write off.
bool render_map(MapRenderer*,rhi::IRenderPassEncoder*,const WorldCamera&,
    WorldRenderer&,bool forward,bool phase2=false);
bool render_maps_forward(std::span<const std::shared_ptr<MapRenderer>>,rhi::IRenderPassEncoder*,
    const WorldCamera&,WorldRenderer&);
// Render opaque/cutout GLB primitives into a directional shadow depth map.
// Blended primitives are skipped so the fallback matches the RT transmission
// contract. The light-space basis uses the same half-span/depth convention as
// the voxel clipmaps.
bool render_map_shadow(MapRenderer*,rhi::IRenderPassEncoder*,
    const std::array<float,4>& center,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward);
bool render_map_local_shadow(MapRenderer*,rhi::IRenderPassEncoder*,
    const std::array<float,4>& position,const std::array<float,4>& projection,
    const std::array<float,4>& right,const std::array<float,4>& up,
    const std::array<float,4>& forward);
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
struct MapRayGeometry {std::uint64_t vertices{},indices{},materials{};};
static_assert(sizeof(MapRayGeometry)==24);
bool map_ray_geometry(const MapRenderer&,MapRayGeometry&);
bool bind_map_ray_buffers(MapRenderer&,rhi::IShaderObject* root);
}
