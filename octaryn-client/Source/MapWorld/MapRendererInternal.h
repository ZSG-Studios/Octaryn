#pragma once
#include "MapRenderer.h"
#include "MapImages.h"
#include "MapModel.h"
#include <slang-com-ptr.h>
#include <filesystem>
#include <vector>
#include <array>
#include <chrono>

namespace octaryn::client::rendering {
// Ray-query material record; layout matches MapGeometry.slang exactly.
struct MapRayMaterial {
  float base_color[4]{1,1,1,1};
  float emissive[3]{},normal_scale{1};
  float metallic{1},roughness{1},alpha_cutoff{},occlusion_strength{1};
  std::uint32_t alpha_mode{},double_sided{},padding[2]{};
  struct Texture {
    std::uint64_t image{},sampler{};
    float transform[6]{1,0,0,0,1,0};
    std::uint32_t texcoord{},present{};
  } textures[5];
};
static_assert(sizeof(MapRayMaterial)==304);
struct MapRenderer {
  std::filesystem::path texture_cache_directory;
  Slang::ComPtr<rhi::IDevice> device;
  MapModel model;
  Slang::ComPtr<rhi::IBuffer> vertices,indices,raster_indices,ray_primitives;
  std::vector<Slang::ComPtr<rhi::ITexture>> textures;
  std::vector<Slang::ComPtr<rhi::ITextureView>> texture_views;
  std::vector<std::array<size_t,5>> material_texture_slots;
  std::vector<Slang::ComPtr<rhi::ISampler>> material_samplers;
  Slang::ComPtr<rhi::IRenderPipeline> gbuffer_pipeline,forward_pipeline,forward_rt_pipeline,
      shadow_pipeline,local_shadow_pipeline;
  Slang::ComPtr<rhi::IAccelerationStructure> blas,tlas;
  Slang::ComPtr<rhi::IBuffer> blas_scratch,tlas_scratch,instances;
  Slang::ComPtr<rhi::IFence> ray_pending_fence;
  Slang::ComPtr<rhi::ICommandBuffer> ray_pending_commands;
  std::chrono::steady_clock::time_point ray_submitted_at{};
  std::vector<std::uint32_t> forward_order;
  bool ray_supported{},ray_ready{},any_double_sided_blend{};
};
bool upload_map_images(MapRenderer&);
bool upload_map_materials(MapRenderer&);
}
