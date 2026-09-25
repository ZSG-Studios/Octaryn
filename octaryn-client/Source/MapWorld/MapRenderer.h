#pragma once
#include <slang-rhi.h>
#include "MapModel.h"
#include <array>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct MapRenderer;
// Loads the flattened GLB scene, uploads buffers/textures and compiles the
// G-buffer plus forward pipelines. Returns null (with a stderr diagnostic) on
// any malformed asset so callers can refuse the world cleanly.
MapRenderer* create_map_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* glb_path,const char* shader_path);
void destroy_map_renderer(MapRenderer*);
const MapModel& map_model(const MapRenderer&);
// Opaque draws write the five G-buffer targets; forward draws blended
// primitives far to near with depth write off.
bool render_map(MapRenderer*,rhi::IRenderPassEncoder*,const WorldCamera&,
    WorldRenderer&,bool forward);
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
bool pump_map_ray_scene(MapRenderer&,rhi::ICommandQueue*,bool requested);
void finish_map_ray_scene(MapRenderer&);
// Records a build without publishing readiness. Caller owns submission/fence.
bool prepare_map_ray_scene(MapRenderer&,rhi::ICommandEncoder*);
bool map_ray_ready(const MapRenderer&);
rhi::IAccelerationStructure* map_ray_blas(const MapRenderer&);
// Tolerant ray-query buffer bindings for passes that include WorldRayQuery.
bool bind_map_ray_buffers(MapRenderer&,rhi::IShaderObject* root);
}
