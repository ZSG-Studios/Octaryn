#pragma once
#include "LocalLight.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <vector>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct LocalLightingSettings {unsigned tile_capacity{64},debug{};};
struct LocalLightingSystem {
  LocalLightingSettings settings;
  std::vector<WorldLocalLight> lights;
  Slang::ComPtr<rhi::IBuffer> light_buffer,counters,tile_counts,tile_lights;
  Slang::ComPtr<rhi::IComputePipeline> tile_pipeline,tile_sort_pipeline,direct_pipeline,fallback_pipeline;
  Slang::ComPtr<rhi::ITexture> output;
  Slang::ComPtr<rhi::ITextureView> output_view;
  unsigned width{},height{},tile_capacity{};
  std::uint64_t light_revision{},uploaded_revision{~0ull},shaded_pixel_count{},visibility_ray_budget{},gpu_bytes{};
  bool active{};
};
bool world_local_lighting_initialize(WorldRenderer&);
bool world_local_lighting_prepare(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_clusters(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_lighting_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_lighting_bind(WorldRenderer&,rhi::IShaderObject*);
}
