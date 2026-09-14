#include "ColumnStreaming.h"

#include "VoxelLayout.h"

#include <algorithm>

namespace octaryn::client::voxel {

StreamingBudget default_streaming_budget() noexcept
{
    return {
        8u,
        4u,
        64u,
        128u,
        32u,
    };
}

std::vector<ColumnRequest> build_required_columns(
    ColumnCoord center,
    std::int32_t render_distance)
{
    if (!is_valid_render_distance(render_distance)) {
        return {};
    }

    std::vector<ColumnRequest> required;
    const auto width = static_cast<std::size_t>(render_distance * 2 + 1);
    required.reserve(width * width);

    for (std::int32_t dz = -render_distance; dz <= render_distance; ++dz) {
        for (std::int32_t dx = -render_distance; dx <= render_distance; ++dx) {
            if (!is_inside_render_square(dx, dz, render_distance)) {
                continue;
            }
            required.push_back({
                {center.x + dx, center.z + dz},
                column_load_priority(dx, dz),
            });
        }
    }

    std::sort(required.begin(), required.end(),
              [](const ColumnRequest& left, const ColumnRequest& right) {
                  return compare_column_load_priority(left.priority,
                                                      right.priority) < 0;
              });
    return required;
}

bool column_state_allows_generation(ColumnState state) noexcept
{
    return state == ColumnState::Missing || state == ColumnState::Requested;
}

bool column_state_allows_eviction(ColumnState state) noexcept
{
    return state == ColumnState::GpuResident || state == ColumnState::UploadQueued;
}

} // namespace octaryn::client::voxel
