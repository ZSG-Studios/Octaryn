#pragma once
#include "WorldBatch.h"
#include <utility>
namespace octaryn::client::rendering {
struct WorldColumnGpu;
struct ShadowBatchRecord {
  std::uint64_t faces{},fluids{};
  std::uint32_t first_face{},fluid_base{};
};
static_assert(sizeof(ShadowBatchRecord)==24);
struct ShadowBatchColumn {
  std::pair<std::int32_t,std::int32_t> coordinate;
  const WorldColumnGpu* column{};
  std::uint64_t faces{},fluids{};
};
struct ShadowBatch {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IRenderPipeline> raster;
  std::array<WorldBatchFrame,2> frames;
  std::vector<ShadowBatchColumn> columns;
  std::vector<ShadowBatchRecord> records;
  std::vector<WorldBatchArguments> arguments;
  std::array<std::uint32_t,3> first{},count{};
  std::uint32_t max_draws{1},submitted_commands{},submitted_draws{};
  unsigned active_slot{};
  bool available{},enabled{true},prepared{};
  std::uint64_t gpu_bytes() const;
  ~ShadowBatch();
};
bool shadow_batch_initialize(WorldRenderer&);
// The renderer completes this frame slot's fence before releasing its owners.
bool shadow_batch_begin_frame(ShadowBatch&,unsigned slot);
bool shadow_column_visible(std::pair<std::int32_t,std::int32_t>,const WorldColumnGpu&,
    const std::array<float,4>& center,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward);
bool shadow_batch_prepare(WorldRenderer&,rhi::ICommandEncoder*,
    const std::array<std::array<float,4>,3>& centers,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward);
bool shadow_batch_draw(WorldRenderer&,rhi::IRenderPassEncoder*,rhi::IShaderObject*,unsigned level);
}
