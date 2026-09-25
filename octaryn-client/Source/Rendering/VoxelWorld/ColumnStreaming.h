#pragma once

#include "RenderDistanceRing.h"

#include <cstdint>
#include <vector>

namespace octaryn::client::voxel {

enum class ColumnState : std::uint8_t {
    Missing,
    Requested,
    Generating,
    GeneratedCpuCompact,
    UploadQueued,
    GpuResident,
    EvictPending,
    FenceWait,
};

struct StreamingBudget {
    std::uint32_t max_new_column_requests;
    std::uint32_t max_generated_columns_accepted;
    std::uint32_t max_chunk_uploads;
    std::uint32_t max_gpu_meshing_chunks;
    std::uint32_t max_evictions;
};

struct ColumnRequest {
    ColumnCoord coord;
    ColumnLoadPriority priority;
};

StreamingBudget default_streaming_budget() noexcept;
std::vector<ColumnRequest> build_required_columns(
    ColumnCoord center,
    std::int32_t render_distance);
bool column_state_allows_generation(ColumnState state) noexcept;
bool column_state_allows_eviction(ColumnState state) noexcept;

} // namespace octaryn::client::voxel
