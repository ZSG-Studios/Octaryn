#pragma once

#include "EmptyWorldMesh.h"

#include <cstdint>

bool empty_world_chunk_range(const chunk_view &view,
                             int32_t &min_chunk_x, int32_t &max_chunk_x,
                             int32_t &min_chunk_z, int32_t &max_chunk_z);
