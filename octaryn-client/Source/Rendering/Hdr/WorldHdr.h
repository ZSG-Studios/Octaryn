#pragma once
#include <slang-rhi.h>
#include <array>
namespace octaryn::client::rendering {
// Original window_textures.cpp and composite.comp.glsl resource contract.
inline constexpr std::array<rhi::Format,4> world_gbuffer_formats{
  rhi::Format::RGBA16Float,rhi::Format::RGBA32Float,rhi::Format::RGBA8Unorm,rhi::Format::RGBA8Unorm};
struct WorldHdr {
  std::array<Slang::ComPtr<rhi::ITexture>,4> gbuffer;
  std::array<Slang::ComPtr<rhi::ITextureView>,4> views;
  Slang::ComPtr<rhi::ITexture> scene;
  Slang::ComPtr<rhi::ITextureView> scene_view;
  Slang::ComPtr<rhi::ITexture> sun_visibility;
  Slang::ComPtr<rhi::ITextureView> sun_visibility_view;
  bool ray_shadows{};
  Slang::ComPtr<rhi::IComputePipeline> composite,composite_rt,composite_src,present;
};
bool create_world_hdr(rhi::IDevice*,WorldHdr&);
bool resize_world_hdr(rhi::IDevice*,WorldHdr&,unsigned width,unsigned height);
struct WorldRenderer;
bool composite_world_hdr(WorldRenderer&,rhi::ICommandEncoder*);
// Optional reconstructed scene is tone-mapped linear; default HDR retains the original mapping.
bool present_world_hdr(rhi::ICommandEncoder*,WorldHdr&,rhi::ITextureView* output,unsigned width,unsigned height,rhi::ITextureView* scene=nullptr);
}
