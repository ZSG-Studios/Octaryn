#pragma once
#include <slang-com-ptr.h>
#include <slang-rhi.h>
#include <cstdint>
#include <vector>
namespace octaryn::client::rendering {
// Hi-Z occlusion pyramid over the G-buffer depth attachment. Built once per
// frame after the phase-1 opaque map pass; the previous frame's pyramid feeds
// phase-1 culling, the fresh pyramid feeds the phase-2 retest.
struct WorldHiz {
  Slang::ComPtr<rhi::ITexture> pyramid;
  std::vector<Slang::ComPtr<rhi::ITextureView>> mip_views;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::ISampler> sampler;
  std::uint32_t width{},height{},mips{};
  bool valid{};
};
bool create_world_hiz(rhi::IDevice*,WorldHiz&);
bool resize_world_hiz(rhi::IDevice*,WorldHiz&,unsigned width,unsigned height);
bool build_world_hiz(WorldHiz&,rhi::ICommandEncoder*,rhi::ITexture* depth);
}
