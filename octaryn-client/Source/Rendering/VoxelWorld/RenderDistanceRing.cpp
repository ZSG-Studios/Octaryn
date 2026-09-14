#include "RenderDistanceRing.h"

#include <algorithm>
#include <cstdlib>

namespace octaryn::client::voxel {
namespace {

std::uint32_t part_16_bits(std::uint32_t value) noexcept
{
    value &= 0x0000ffffu;
    value = (value | (value << 8u)) & 0x00ff00ffu;
    value = (value | (value << 4u)) & 0x0f0f0f0fu;
    value = (value | (value << 2u)) & 0x33333333u;
    value = (value | (value << 1u)) & 0x55555555u;
    return value;
}

std::uint32_t zigzag(std::int32_t value) noexcept
{
    return static_cast<std::uint32_t>((value << 1) ^ (value >> 31));
}

} // namespace

bool is_inside_render_square(
    std::int32_t dx,
    std::int32_t dz,
    std::int32_t render_distance) noexcept
{
    return std::max(std::abs(dx), std::abs(dz)) <= render_distance;
}

ColumnLoadPriority column_load_priority(
    std::int32_t dx,
    std::int32_t dz) noexcept
{
    const auto x = zigzag(dx);
    const auto z = zigzag(dz);
    return {
        std::max(std::abs(dx), std::abs(dz)),
        std::abs(dx) + std::abs(dz),
        part_16_bits(x) | (part_16_bits(z) << 1u),
    };
}

int compare_column_load_priority(
    const ColumnLoadPriority& left,
    const ColumnLoadPriority& right) noexcept
{
    if (left.ring != right.ring) {
        return left.ring < right.ring ? -1 : 1;
    }
    if (left.manhattan != right.manhattan) {
        return left.manhattan < right.manhattan ? -1 : 1;
    }
    if (left.morton != right.morton) {
        return left.morton < right.morton ? -1 : 1;
    }
    return 0;
}

} // namespace octaryn::client::voxel
