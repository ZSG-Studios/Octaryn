#include "PackedVoxelQuad.h"

namespace octaryn::client::voxel {
namespace {

constexpr std::uint32_t mask(std::uint32_t bits) noexcept
{
    return (1u << bits) - 1u;
}

std::uint32_t field(
    std::uint32_t value,
    std::uint32_t bits,
    std::uint32_t shift) noexcept
{
    return (value & mask(bits)) << shift;
}

std::uint32_t read(
    std::uint32_t value,
    std::uint32_t bits,
    std::uint32_t shift) noexcept
{
    return (value >> shift) & mask(bits);
}

} // namespace

bool voxel_quad16_fields_valid(const DecodedVoxelQuad& quad) noexcept
{
    return quad.local_x < 32u && quad.local_y < 32u &&
           quad.local_z < 32u && quad.width >= 1u && quad.width <= 32u &&
           quad.height >= 1u && quad.height <= 32u &&
           quad.face_dir < 6u && quad.lod_tier < 4u &&
           quad.material_id < (1u << 20u) && quad.alpha_mode < 16u &&
           quad.render_flags < 256u && quad.uv_rotation < 16u &&
           quad.texture_variant < 16u && quad.chunk_local_index < 65536u &&
           quad.material_bin < 256u;
}

PackedVoxelQuad16 pack_voxel_quad16(const DecodedVoxelQuad& quad) noexcept
{
    return {
        field(quad.local_x, 5u, 0u) |
            field(quad.local_y, 5u, 5u) |
            field(quad.local_z, 5u, 10u) |
            field(quad.width - 1u, 5u, 15u) |
            field(quad.height - 1u, 5u, 20u) |
            field(quad.face_dir, 3u, 25u) |
            field(quad.lod_tier, 2u, 28u),
        field(quad.material_id, 20u, 0u) |
            field(quad.alpha_mode, 4u, 20u) |
            field(quad.render_flags, 8u, 24u),
        field(quad.uv_rotation, 4u, 24u) |
            field(quad.texture_variant, 4u, 28u),
        field(quad.chunk_local_index, 16u, 0u) |
            field(quad.material_bin, 8u, 16u),
    };
}

DecodedVoxelQuad unpack_voxel_quad16(const PackedVoxelQuad16& packed) noexcept
{
    return {
        read(packed.pos_size_dir_lod, 5u, 0u),
        read(packed.pos_size_dir_lod, 5u, 5u),
        read(packed.pos_size_dir_lod, 5u, 10u),
        read(packed.pos_size_dir_lod, 5u, 15u) + 1u,
        read(packed.pos_size_dir_lod, 5u, 20u) + 1u,
        read(packed.pos_size_dir_lod, 3u, 25u),
        read(packed.pos_size_dir_lod, 2u, 28u),
        read(packed.material_flags, 20u, 0u),
        read(packed.material_flags, 4u, 20u),
        read(packed.material_flags, 8u, 24u),
        read(packed.light_ao_uv, 4u, 24u),
        read(packed.light_ao_uv, 4u, 28u),
        read(packed.chunk_batch_meta, 16u, 0u),
        read(packed.chunk_batch_meta, 8u, 16u),
    };
}

} // namespace octaryn::client::voxel
