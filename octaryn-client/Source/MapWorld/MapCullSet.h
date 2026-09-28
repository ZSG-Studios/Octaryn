#pragma once
#include <slang-com-ptr.h>
#include <slang-rhi.h>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
namespace octaryn::client::rendering {
struct MapRenderer;
struct WorldCamera;
struct WorldRenderer;
// Fused GPU cull dispatch over every resident indirect map (Wihlidal,
// "GPU-Driven Rendering Pipelines", Frostbite). Each map owns a contiguous run
// of 256-entry slots in the global primitive/argument/flag tables, so one
// compute dispatch per phase replaces one dispatch per map.
struct MapCullSet {
  static constexpr std::uint32_t kSlots=512;
  static constexpr std::uint32_t kSlotPrimitives=256;
  static constexpr std::uint32_t kSlotArguments=256*5;
  Slang::ComPtr<rhi::IBuffer> primitives,arguments_a,arguments_b,flags;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  std::vector<MapRenderer*> members;
  std::uint32_t covered{};
  bool occlusion{};
};
bool create_map_cull_set(rhi::IDevice*,MapCullSet&);
// Registers new resident indirect maps and copies each new map's primitive
// table into its slot. Call once per frame with the same encoder that recorded
// the map uploads, before dispatch_map_cull.
bool sync_map_cull_set(MapCullSet&,rhi::ICommandEncoder*,std::span<const std::shared_ptr<MapRenderer>>);
bool dispatch_map_cull(MapCullSet&,rhi::ICommandEncoder*,const WorldCamera&,WorldRenderer&,unsigned phase);
}
