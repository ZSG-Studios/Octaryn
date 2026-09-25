#pragma once

#include <cstdint>

namespace octaryn::client::voxel {

struct PackedVoxelQuad16 {
    std::uint32_t pos_size_dir_lod;
    std::uint32_t material_flags;
    std::uint32_t light_ao_uv;
    std::uint32_t chunk_batch_meta;
};

struct DecodedVoxelQuad {
    std::uint32_t local_x;
    std::uint32_t local_y;
    std::uint32_t local_z;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t face_dir;
    std::uint32_t lod_tier;
    std::uint32_t material_id;
    std::uint32_t alpha_mode;
    std::uint32_t render_flags;
    std::uint32_t uv_rotation;
    std::uint32_t texture_variant;
    std::uint32_t chunk_local_index;
    std::uint32_t material_bin;
};

PackedVoxelQuad16 pack_voxel_quad16(const DecodedVoxelQuad& quad) noexcept;
DecodedVoxelQuad unpack_voxel_quad16(const PackedVoxelQuad16& packed) noexcept;
bool voxel_quad16_fields_valid(const DecodedVoxelQuad& quad) noexcept;

} // namespace octaryn::client::voxel
