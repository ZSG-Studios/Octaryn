#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <vector>
#include <cstdint>
namespace octaryn::client::rendering {
struct WorldRenderer;
constexpr std::uint32_t WorldBatchMaxColumns=65*65;
constexpr std::uint32_t WorldBatchDescriptorCapacity=32768;
static_assert(WorldBatchDescriptorCapacity>=2*WorldBatchMaxColumns*2);
struct WorldBatchRecord {
  std::uint64_t faces{},patches{};
  std::uint32_t patch_base{},padding{};
};
static_assert(sizeof(WorldBatchRecord)==24);
struct WorldBatchArguments {std::uint32_t vertices{},instances{},first_vertex{},first_instance{};};
static_assert(sizeof(WorldBatchArguments)==16);
struct WorldBatchFrame {
  Slang::ComPtr<rhi::IBuffer> records,arguments;
  void* mapped_records{};void* mapped_arguments{};
  std::vector<Slang::ComPtr<rhi::IBuffer>> retained;
};
struct WorldBatch {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IRenderPipeline> opaque,sprite;
  std::array<WorldBatchFrame,2> frames;
  unsigned active_slot{};
  std::vector<WorldBatchRecord> cpu_records;
  std::vector<WorldBatchArguments> cpu_arguments;
  std::array<std::uint32_t,2> first{},count{};
  std::uint32_t max_draws{1},submitted_commands{},submitted_columns{};
  bool available{},required{},prepared{},enabled{true};
  WorldBatchFrame& frame() {return frames[active_slot];}
  const WorldBatchFrame& frame() const {return frames[active_slot];}
  std::uint64_t gpu_bytes() const;
  ~WorldBatch();
};
bool world_batch_initialize(WorldRenderer&,bool descriptor_capacity_available=true);
// Caller must complete this slot's previous submission before selecting it.
bool world_batch_begin_frame(WorldRenderer&,unsigned slot);
bool world_batch_prepare(WorldRenderer&,rhi::ICommandEncoder*);
bool world_batch_draw(WorldRenderer&,rhi::IRenderPassEncoder*,std::size_t pass);
}
