#pragma once
#include <slang-rhi.h>
#include <array>
#include <span>
#include <cstdint>

namespace octaryn::client::rendering {

struct WorldRenderer;

// Dynamic receiver probes fed the archived block-transport GI. Only the
// accounting surface remains for renderer stats; the player's indirect receive
// path returns to life with the new world geometry streaming.
inline constexpr unsigned DynamicReceiverCapacity = 8192, DynamicReceiverRayBudget = 32768;

struct DynamicReceiver {
  std::array<float, 4> position{}, normal{};
  std::array<std::uint32_t, 4> identity{};
};
static_assert(sizeof(DynamicReceiver) == 48);

struct DynamicReceivers {
  std::uint64_t gpu_bytes{};
  unsigned count{}, samples{};
  bool active{};
};

inline bool initialize_dynamic_receivers(DynamicReceivers&, WorldRenderer&) { return true; }
inline bool prepare_dynamic_receivers(DynamicReceivers&, WorldRenderer&, rhi::ICommandEncoder*,
                                      std::span<const DynamicReceiver>) { return true; }
inline bool bind_dynamic_receivers(DynamicReceivers&, WorldRenderer&, rhi::IShaderObject*) { return true; }

}
