#pragma once
#include <cstdint>
#include <slang-rhi.h>
namespace octaryn::client::rendering {
enum class LightingTier { RasterFallback, HybridRT, FullRT };
struct RendererCapabilities {
  LightingTier lighting_tier{LightingTier::RasterFallback};
  bool acceleration_structures{},inline_ray_queries{},ray_pipelines{},hardware_ray_tracing{};
  bool bindless{},wave_operations{},indirect_draw{},indirect_dispatch{};
  bool multi_draw_indirect{},draw_indirect_first_instance{},shader_draw_parameters{};
  bool fp16{},atomic_float{},atomic_fp16{},atomic_int64{},timestamps{};
  std::uint64_t max_buffer_bytes{};
  // RHI reports buffer allocation limits, but no distinct storage binding limit.
  std::uint64_t max_storage_binding_bytes{};
  std::uint32_t buffer_descriptors{},texture_descriptors{},sampler_descriptors{};
  std::uint32_t max_visible_samplers{},min_wave_size{},max_wave_size{},max_indirect_draws{};
  bool inline_lighting() const {return acceleration_structures && inline_ray_queries && bindless;}
};
RendererCapabilities renderer_capabilities(rhi::IDevice*,const rhi::BindlessDesc&);
void print_renderer_capabilities(const RendererCapabilities&);
}
