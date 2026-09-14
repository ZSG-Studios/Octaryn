#pragma once
#include <slang-rhi.h>
#include <array>
namespace octaryn::client::rendering {
// Original window_textures.cpp and composite.comp.glsl resource contract.
inline constexpr std::array<rhi::Format,4> world_gbuffer_formats{
  rhi::Format::RGBA16Float,rhi::Format::RGBA16Float,rhi::Format::RGBA8Unorm,rhi::Format::RGBA8Unorm};
struct WorldHdr {
  std::array<Slang::ComPtr<rhi::ITexture>,4> gbuffer;
  std::array<Slang::ComPtr<rhi::ITextureView>,4> views;
  Slang::ComPtr<rhi::ITexture> scene;
  Slang::ComPtr<rhi::ITextureView> scene_view;
  Slang::ComPtr<rhi::IComputePipeline> composite,present;
};
bool create_world_hdr(rhi::IDevice*,WorldHdr&);
bool resize_world_hdr(rhi::IDevice*,WorldHdr&,unsigned width,unsigned height);
bool composite_world_hdr(rhi::ICommandEncoder*,WorldHdr&,float sky,float ambient,float twilight,float fog_distance,const float sun[4],unsigned width,unsigned height);
// Optional reconstructed scene is tone-mapped linear; default HDR retains the original mapping.
bool present_world_hdr(rhi::ICommandEncoder*,WorldHdr&,rhi::ITextureView* output,unsigned width,unsigned height,rhi::ITextureView* scene=nullptr);
}
