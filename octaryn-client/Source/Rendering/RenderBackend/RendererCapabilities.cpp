#include "RendererCapabilities.h"
#include <cstdio>
namespace octaryn::client::rendering {
RendererCapabilities renderer_capabilities(rhi::IDevice* device,const rhi::BindlessDesc& descriptors) {
  RendererCapabilities c;
  if(!device)return c;
  const auto feature=[device](rhi::Feature f){return device->hasFeature(f);};
  c.acceleration_structures=feature(rhi::Feature::AccelerationStructure);
  c.inline_ray_queries=feature(rhi::Feature::RayQuery);
  c.ray_pipelines=feature(rhi::Feature::RayTracing);
  c.hardware_ray_tracing=c.acceleration_structures && (c.inline_ray_queries || c.ray_pipelines);
  c.bindless=feature(rhi::Feature::Bindless);
  c.wave_operations=feature(rhi::Feature::WaveOps);
  c.multi_draw_indirect=feature(rhi::Feature::MultiDrawIndirect);
  c.draw_indirect_first_instance=feature(rhi::Feature::DrawIndirectFirstInstance);
  c.shader_draw_parameters=feature(rhi::Feature::ShaderDrawParameters);
  // Single indirect draw/dispatch are core RHI raster/compute commands.
  c.indirect_draw=feature(rhi::Feature::Rasterization);
  c.indirect_dispatch=device->getInfo().limits.maxComputeThreadsPerGroup>0;
  c.fp16=feature(rhi::Feature::Half);c.atomic_float=feature(rhi::Feature::AtomicFloat);
  c.atomic_fp16=feature(rhi::Feature::AtomicHalf);c.atomic_int64=feature(rhi::Feature::AtomicInt64);
  c.timestamps=feature(rhi::Feature::TimestampQuery);
  const auto& limits=device->getInfo().limits;
  c.max_buffer_bytes=limits.maxBufferSize;c.max_visible_samplers=limits.maxShaderVisibleSamplers;
  c.min_wave_size=limits.minWaveSize;c.max_wave_size=limits.maxWaveSize;c.max_indirect_draws=limits.maxDrawIndirectCount;
  if(c.bindless) {
    c.buffer_descriptors=descriptors.bufferCount;c.texture_descriptors=descriptors.textureCount;
    c.sampler_descriptors=descriptors.samplerCount;
  }
  if(c.inline_lighting())c.lighting_tier=c.wave_operations?LightingTier::FullRT:LightingTier::HybridRT;
  return c;
}
void print_renderer_capabilities(const RendererCapabilities& c) {
  std::printf("world_capabilities tier=%u as=%u ray_query=%u ray_pipeline=%u bindless=%u wave=%u fp16=%u atomic_float=%u atomic_half=%u atomic_int64=%u max_buffer_bytes=%llu storage_binding_limit=unreported buffer_descriptors=%u texture_descriptors=%u sampler_descriptors=%u\n",
    static_cast<unsigned>(c.lighting_tier),c.acceleration_structures,c.inline_ray_queries,c.ray_pipelines,c.bindless,
    c.wave_operations,c.fp16,c.atomic_float,c.atomic_fp16,c.atomic_int64,static_cast<unsigned long long>(c.max_buffer_bytes),
    c.buffer_descriptors,c.texture_descriptors,c.sampler_descriptors);
}
}
