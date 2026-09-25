#pragma once
#include <slang-rhi.h>
#include <array>
namespace octaryn::client::rendering {
// Original window_textures.cpp and composite.comp.glsl resource contract.
inline constexpr std::array<rhi::Format,6> world_gbuffer_formats{
  rhi::Format::RGBA16Float,rhi::Format::RGBA32Float,rhi::Format::RGBA8Unorm,rhi::Format::RGBA8Unorm,
  rhi::Format::RGBA16Float,rhi::Format::RGBA32Uint};
inline constexpr unsigned world_gbuffer_count=5;
inline unsigned world_gbuffer_target_count(bool block_transport) {return world_gbuffer_count+(block_transport?1u:0u);}
struct WorldHdr {
  std::array<Slang::ComPtr<rhi::ITexture>,world_gbuffer_formats.size()> gbuffer;
  std::array<Slang::ComPtr<rhi::ITextureView>,world_gbuffer_formats.size()> views;
  Slang::ComPtr<rhi::ITexture> scene;
  Slang::ComPtr<rhi::ITextureView> scene_view;
  Slang::ComPtr<rhi::ITexture> sun_visibility;
  Slang::ComPtr<rhi::ITextureView> sun_visibility_view;
  bool ray_shadows{};
  bool block_transport{};
  Slang::ComPtr<rhi::IComputePipeline> composite,composite_rt,present;
};
bool create_world_hdr(rhi::IDevice*,WorldHdr&,bool block_transport=false);
bool resize_world_hdr(rhi::IDevice*,WorldHdr&,unsigned width,unsigned height);
struct WorldRenderer;
bool composite_world_hdr(WorldRenderer&,rhi::ICommandEncoder*);
// Optional reconstructed scene is tone-mapped linear; default HDR retains the original mapping.
bool present_world_hdr(rhi::ICommandEncoder*,WorldHdr&,rhi::ITextureView* output,unsigned width,unsigned height,rhi::ITextureView* scene=nullptr);
}
