#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <span>
#include <cstdint>

namespace octaryn::client::rendering {
inline constexpr unsigned DynamicReceiverCapacity=8192,DynamicReceiverRayBudget=32768;
struct DynamicReceiver {
  std::array<float,4> position{},normal{};
  std::array<std::uint32_t,4> identity{};
};
static_assert(sizeof(DynamicReceiver)==48);
struct DynamicReceivers {
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> input,previous,values,direct,state,counters,empty;
  std::uint64_t gpu_bytes{};
  unsigned count{},samples{};
  bool active{};
};
struct WorldRenderer;
bool initialize_dynamic_receivers(DynamicReceivers&,WorldRenderer&);
bool prepare_dynamic_receivers(DynamicReceivers&,WorldRenderer&,rhi::ICommandEncoder*,std::span<const DynamicReceiver>);
bool bind_dynamic_receivers(DynamicReceivers&,WorldRenderer&,rhi::IShaderObject*);
}
