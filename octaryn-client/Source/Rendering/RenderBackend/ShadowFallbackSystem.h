#pragma once
#include <slang-rhi.h>
#include <array>
#include "ShadowBatch.h"
namespace octaryn::client::rendering {
struct WorldRenderer;
struct ShadowFallbackSystem {
  ShadowBatch batch;
  Slang::ComPtr<rhi::IRenderPipeline> raster;
  Slang::ComPtr<rhi::IComputePipeline> resolve;
  std::array<Slang::ComPtr<rhi::ITexture>,3> depth;
  std::array<Slang::ComPtr<rhi::ITextureView>,3> views;
  unsigned resolution{};
};
bool initialize_shadow_fallback(WorldRenderer&);
bool update_shadow_fallback(WorldRenderer&,rhi::ICommandEncoder*);
}
