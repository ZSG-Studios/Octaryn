#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct LocalShadowSystem {
  Slang::ComPtr<rhi::IRenderPipeline> raster;
  std::array<Slang::ComPtr<rhi::ITexture>,6> depth;
  std::array<Slang::ComPtr<rhi::ITextureView>,6> views;
  std::array<float,4> position{},projection{};
  unsigned resolution{256},allocated_resolution{},selected{~0u};
  float max_range{64},origin_bias{.025f};
  std::uint64_t scene_revision{},light_revision{},map_updates{},draws{};
  bool valid{};
};
bool initialize_local_shadows(WorldRenderer&);
bool update_local_shadows(WorldRenderer&,rhi::ICommandEncoder*);
bool bind_local_shadows(WorldRenderer&,rhi::IShaderObject*);
}
