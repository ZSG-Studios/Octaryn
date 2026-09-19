#pragma once
#include "MapRenderer.h"
#include "MapImages.h"
#include "MapModel.h"
#include <slang-com-ptr.h>
#include <vector>

namespace octaryn::client::rendering {
// Ray-query material record; layout matches MapGeometry.slang exactly.
struct MapRayMaterial {
  float base_color[4]{1,1,1,1};
  float alpha_cutoff{};
  std::uint32_t alpha_mode{};
};
static_assert(sizeof(MapRayMaterial)==24);
struct MapRenderer {
  Slang::ComPtr<rhi::IDevice> device;
  MapModel model;
  Slang::ComPtr<rhi::IBuffer> vertices,indices,ray_primitives,ray_triangle_primitives;
  std::vector<Slang::ComPtr<rhi::ITexture>> textures;
  std::vector<Slang::ComPtr<rhi::ITextureView>> texture_views;
  Slang::ComPtr<rhi::ITexture> white;
  Slang::ComPtr<rhi::ITextureView> white_view;
  Slang::ComPtr<rhi::ISampler> sampler;
  Slang::ComPtr<rhi::IRenderPipeline> gbuffer_pipeline,forward_pipeline;
  Slang::ComPtr<rhi::IAccelerationStructure> blas,tlas;
  Slang::ComPtr<rhi::IBuffer> blas_scratch,tlas_scratch,instances;
  std::vector<std::uint32_t> forward_order;
  bool ray_supported{},ray_ready{},any_double_sided_blend{};
};
bool upload_map_images(MapRenderer&);
}
