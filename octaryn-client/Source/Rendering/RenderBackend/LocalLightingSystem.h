#pragma once
#include "LocalLight.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <optional>
#include <vector>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct LocalLightingSettings {unsigned tile_capacity{64},debug{};};
struct LocalLightFrame {
  Slang::ComPtr<rhi::IBuffer> buffer;
  std::optional<std::uint64_t> uploaded_revision;
  unsigned capacity{};
};
struct LocalLightingSystem {
  LocalLightingSettings settings;
  std::vector<WorldLocalLight> lights;
  // A slot is writable only after its world-frame completion fence.
  std::array<LocalLightFrame,2> light_frames;
  // Active-slot binding shared by direct lighting and secondary transport.
  Slang::ComPtr<rhi::IBuffer> light_buffer,counters,tile_counts,tile_lights;
  Slang::ComPtr<rhi::IComputePipeline> tile_pipeline,tile_sort_pipeline,direct_pipeline,fallback_pipeline;
  Slang::ComPtr<rhi::ITexture> output;
  Slang::ComPtr<rhi::ITextureView> output_view;
  unsigned width{},height{},tile_capacity{};
  std::uint64_t light_revision{},shaded_pixel_count{},visibility_ray_budget{},gpu_bytes{};
  bool active{};
};
bool world_local_lighting_initialize(WorldRenderer&);
bool world_local_lighting_prepare(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_clusters(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_lighting_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_lighting_bind(WorldRenderer&,rhi::IShaderObject*);
}
