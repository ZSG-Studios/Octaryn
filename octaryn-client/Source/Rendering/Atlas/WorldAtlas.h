#pragma once
#include <slang-rhi.h>
#include <array>
#include <cstdint>
#include <span>
namespace octaryn::client::rendering {
struct WorldAtlas;
inline constexpr std::uint64_t WorldAtlasPlantMaskBytes=29ull*(32*32+2)*16;
struct BlockEmission { std::array<float,4> radiance_range{}; bool occludes{},sprite{}; std::uint8_t skylight_opacity{15}; };
std::span<const BlockEmission> world_atlas_emissions(WorldAtlas*);
std::span<const std::uint32_t> world_atlas_flags(WorldAtlas*);
WorldAtlas* create_world_atlas(rhi::IDevice*);
unsigned world_atlas_preview_layer(WorldAtlas*, unsigned block);
void destroy_world_atlas(WorldAtlas*);
bool update_world_atlas(WorldAtlas*,rhi::ICommandEncoder*,double seconds);
void commit_world_atlas(WorldAtlas*);
bool bind_world_atlas(WorldAtlas*,rhi::IShaderObject*);
bool prepare_world_atlas_plant_masks(WorldAtlas*);
std::uint64_t world_atlas_plant_mask_bytes(WorldAtlas*);
rhi::IBuffer* world_atlas_materials(WorldAtlas*);
rhi::ITextureView* world_atlas_albedo(WorldAtlas*);
rhi::ISampler* world_atlas_nearest(WorldAtlas*);
rhi::ISampler* world_atlas_sprite(WorldAtlas*);
}
