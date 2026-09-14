#pragma once
#include "LocalLight.h"
#include "TemporalCamera.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <vector>
namespace octaryn::client::rendering {
struct ReSTIRDISettings {
  unsigned candidates{8},spatial_samples{4},history_limit{32},tile_capacity{64},debug{};
  float spatial_radius{12},position_threshold{.15f},normal_threshold{.9f};
  bool temporal{true},spatial{true};
};
struct ReSTIRDISystem {
  ReSTIRDISettings settings;
  std::vector<WorldLocalLight> lights;
  Slang::ComPtr<rhi::IBuffer> light_buffer,initial,temporal,spatial,history,surfaces;
  // Diagnostic readback only: traced rays, shaded reservoirs, temporal and spatial acceptance.
  Slang::ComPtr<rhi::IBuffer> counters;
  Slang::ComPtr<rhi::IBuffer> tile_counts,tile_lights;
  Slang::ComPtr<rhi::IComputePipeline> tile_pipeline;
  unsigned tile_capacity{};
  Slang::ComPtr<rhi::ITexture> output;
  Slang::ComPtr<rhi::ITextureView> output_view;
  Slang::ComPtr<rhi::IComputePipeline> initial_pipeline,temporal_pipeline,spatial_pipeline,visibility_pipeline,fallback_pipeline;
  TemporalView previous;
  std::array<float,4> previous_jitter{};
  unsigned width{},height{};
  std::uint64_t scene_revision{},light_revision{},uploaded_revision{~0ull};
  std::uint64_t reservoir_count{},visibility_ray_budget{},gpu_bytes{};
  bool history_valid{},active{};
};
bool world_restir_initialize(WorldRenderer&);
bool world_restir_prepare_lights(WorldRenderer&,rhi::ICommandEncoder*);
bool world_local_clusters(WorldRenderer&,rhi::ICommandEncoder*);
bool world_restir_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_restir_bind(WorldRenderer&,rhi::IShaderObject*);
}
