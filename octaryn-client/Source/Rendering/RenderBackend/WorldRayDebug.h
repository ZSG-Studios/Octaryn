#pragma once
#include <slang-rhi.h>
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldRayDebug {
  Slang::ComPtr<rhi::IComputePipeline> trace;
  Slang::ComPtr<rhi::IRenderPipeline> lines;
  struct Frame {
    Slang::ComPtr<rhi::IBuffer> bounds;
    std::uint64_t generation{};
    std::uint32_t count{};
  };
  std::array<Frame,2> frames;
};
bool world_ray_debug_initialize(WorldRenderer&);
bool world_ray_debug(WorldRenderer&,rhi::ICommandEncoder*);
}
