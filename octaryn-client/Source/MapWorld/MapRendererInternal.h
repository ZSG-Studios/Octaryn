#pragma once
#include "MapRenderer.h"
#include "MapImages.h"
#include "MapModel.h"
#include <slang-com-ptr.h>
#include <filesystem>
#include <vector>
#include <array>
#include <chrono>
#include <memory>
#include <future>

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
struct MapTextureResource;
struct MapSamplerCache;
struct MapSamplerResource;
struct MapRenderer {
  std::uint64_t texture_bytes{};
  std::filesystem::path texture_cache_directory;
  Slang::ComPtr<rhi::IDevice> device;
  MapModel model;
  std::uint32_t vertex_count{},index_count{};
  Slang::ComPtr<rhi::IBuffer> vertices,indices,raster_indices,ray_primitives;
  Slang::ComPtr<rhi::IBuffer> indirect_primitives;
  std::int32_t cull_slot{-1};
  bool occlusion_enabled{};
  Slang::ComPtr<rhi::IBuffer> lod_indices;
  Slang::ComPtr<rhi::IBuffer> meshlets,meshlet_vertices,meshlet_triangles;
  Slang::ComPtr<rhi::IRenderPipeline> meshlet_pipeline;
  std::uint32_t meshlet_count{};
  bool meshlet_enabled{};
  Slang::ComPtr<rhi::IRenderPipeline> indirect_gbuffer_pipeline;
  bool indirect_enabled{};
  float lod_pixel_error{};
  std::vector<Slang::ComPtr<rhi::ITexture>> textures;
  std::vector<std::shared_ptr<MapTextureResource>> texture_resources;
  std::vector<Slang::ComPtr<rhi::ITextureView>> texture_views;
  std::vector<std::array<size_t,5>> material_texture_slots;
  std::shared_ptr<MapSamplerCache> sampler_cache;
  std::vector<std::shared_ptr<MapSamplerResource>> material_samplers;
  Slang::ComPtr<rhi::IRenderPipeline> gbuffer_pipeline,forward_pipeline,forward_rt_pipeline,
      shadow_pipeline,local_shadow_pipeline;
  Slang::ComPtr<rhi::IAccelerationStructure> blas,tlas,uncompacted_blas;
  Slang::ComPtr<rhi::IQueryPool> compact_size;
  Slang::ComPtr<rhi::IBuffer> blas_scratch,tlas_scratch,instances;
  Slang::ComPtr<rhi::IFence> ray_pending_fence;
  Slang::ComPtr<rhi::ICommandBuffer> ray_pending_commands;
  std::chrono::steady_clock::time_point ray_submitted_at{};
  bool ray_supported{},ray_ready{},any_double_sided_blend{};
  bool ray_resources_ready{};
  std::future<Slang::ComPtr<rhi::IAccelerationStructure>> compact_allocation;
  bool ray_build_completed{};
};
bool submit_map_ray_compaction(MapRenderer&,rhi::ICommandQueue*,bool asynchronous_allocation=true,
    const MapRaySubmitScope* profile=nullptr);
bool upload_map_images(MapRenderer&);
bool create_map_indirect_buffers(MapRenderer&);
bool create_map_meshlet_buffers(MapRenderer&);
bool upload_map_materials(MapRenderer&);
}
