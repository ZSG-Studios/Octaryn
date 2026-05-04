#pragma once

#include "EmptyWorldMesh.h"

#include <cstddef>
#include <cstdint>

bool empty_world_chunk_range(const chunk_view &view,
                             int32_t &min_chunk_x, int32_t &max_chunk_x,
                             int32_t &min_chunk_z, int32_t &max_chunk_z);
size_t empty_world_chunk_count(const chunk_view &view);
size_t empty_world_chunk_overlap(const chunk_view &left,
                                 const chunk_view &right);
