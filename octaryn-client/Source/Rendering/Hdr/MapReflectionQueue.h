#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
struct MapReflectionQueue {
  std::array<Slang::ComPtr<rhi::IComputePipeline>,8> pipelines;
  Slang::ComPtr<rhi::IComputePipeline> screen;
  Slang::ComPtr<rhi::IRenderPipeline> coverage,item_coverage;
  Slang::ComPtr<rhi::IBuffer> receivers,recovery,counts,arguments,intersections;
  Slang::ComPtr<rhi::IBuffer> recovery_samples,recovery_high,recovery_low;
  std::uint64_t bytes{};
  unsigned capacity{};
  unsigned recovery_ray_budget{},feedback_samples{};
  float recovery_budget_scale{1};
  bool enabled{},reference_recovery{},coherent_recovery{},screen_enabled{};
};
struct WorldRenderer;
bool prepare_map_reflection_queue(WorldRenderer&,unsigned width,unsigned height);
bool render_map_reflection_queue(WorldRenderer&,rhi::ICommandEncoder*,bool valid,const float* dimensions);
}
