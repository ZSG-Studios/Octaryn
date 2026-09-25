#pragma once
#include <slang-rhi.h>
#include <cstdint>

namespace octaryn::client::rendering {

// Minimal always-valid material atlas: empty texture layers, zeroed material
// table and plant-mask buffers. It exists so every shader contract that binds
// the atlas keeps compiling and binding without block content; voxel-era
// content loading is gone with the voxel world.
struct WorldAtlas;
inline constexpr std::uint64_t WorldAtlasPlantMaskBytes = 29ull * (32 * 32 + 2) * 16;

WorldAtlas* create_world_atlas(rhi::IDevice*);
void destroy_world_atlas(WorldAtlas*);
bool update_world_atlas(WorldAtlas*, rhi::ICommandEncoder*, double seconds);
void commit_world_atlas(WorldAtlas*);
bool bind_world_atlas(WorldAtlas*, rhi::IShaderObject*);
bool prepare_world_atlas_plant_masks(WorldAtlas*);
std::uint64_t world_atlas_plant_mask_bytes(WorldAtlas*);
rhi::IBuffer* world_atlas_materials(WorldAtlas*);

}
