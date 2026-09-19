#pragma once
#include <slang-rhi.h>
#include "MapModel.h"

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
// Opaque draws write the four G-buffer targets; forward draws blended
// primitives far to near with depth write off.
bool render_map(MapRenderer*,rhi::IRenderPassEncoder*,const WorldCamera&,
    const WorldRenderer&,bool forward);
// One-time static BLAS/TLAS build inside the frame's acceleration pass.
bool prepare_map_ray_scene(MapRenderer&,rhi::ICommandEncoder*);
bool map_ray_ready(const MapRenderer&);
rhi::IAccelerationStructure* map_ray_blas(const MapRenderer&);
// Tolerant ray-query buffer bindings for passes that include WorldRayQuery.
bool bind_map_ray_buffers(MapRenderer&,rhi::IShaderObject* root);
}
