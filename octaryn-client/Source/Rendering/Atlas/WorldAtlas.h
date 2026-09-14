#pragma once
#include <slang-rhi.h>
#include <array>
#include <span>
namespace octaryn::client::rendering {
struct WorldAtlas;
struct BlockEmission { std::array<float,4> radiance_range{}; bool occludes{},sprite{}; };
std::span<const BlockEmission> world_atlas_emissions(WorldAtlas*);
WorldAtlas* create_world_atlas(rhi::IDevice*);
unsigned world_atlas_preview_layer(WorldAtlas*, unsigned block);
void destroy_world_atlas(WorldAtlas*);
bool update_world_atlas(WorldAtlas*,rhi::ICommandQueue*,double seconds);
bool bind_world_atlas(WorldAtlas*,rhi::IShaderObject*);
rhi::IBuffer* world_atlas_materials(WorldAtlas*);
rhi::ITextureView* world_atlas_albedo(WorldAtlas*);
rhi::ISampler* world_atlas_nearest(WorldAtlas*);
rhi::ISampler* world_atlas_sprite(WorldAtlas*);
}
