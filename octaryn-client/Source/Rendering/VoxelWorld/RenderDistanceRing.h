#pragma once

#include <cstdint>

namespace octaryn::client::voxel {

struct ColumnCoord {
    std::int32_t x;
    std::int32_t z;
};

struct ColumnLoadPriority {
    std::int32_t ring;
    std::int32_t manhattan;
    std::uint32_t morton;
};

bool is_inside_render_square(
    std::int32_t dx,
    std::int32_t dz,
    std::int32_t render_distance) noexcept;
ColumnLoadPriority column_load_priority(
    std::int32_t dx,
    std::int32_t dz) noexcept;
int compare_column_load_priority(
    const ColumnLoadPriority& left,
    const ColumnLoadPriority& right) noexcept;

} // namespace octaryn::client::voxel
