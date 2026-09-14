#pragma once
#include <slang-rhi.h>
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct RTShadowSystem {
  Slang::ComPtr<rhi::IComputePipeline> trace,filter;
  struct History {
    Slang::ComPtr<rhi::ITexture> raw,shadow,position,voxel;
    Slang::ComPtr<rhi::ITextureView> raw_view,shadow_view,position_view,voxel_view;
  };
  std::array<History,2> history;
  std::array<float,20> previous_view{};
  unsigned width{},height{},index{},active_width{},active_height{};
  bool valid{};
  std::uint64_t revision{},rays{};
  std::array<float,3> sun{};
};
bool initialize_rt_shadows(WorldRenderer&);
bool update_rt_shadows(WorldRenderer&,rhi::ICommandEncoder*);
}
