#pragma once

#include "EmptyWorldMesh.h"

#include <cstddef>
#include <cstdint>

bool empty_world_chunk_range(const octaryn_client_chunk_view &chunk_view,
                             int32_t &min_chunk_x, int32_t &max_chunk_x,
                             int32_t &min_chunk_z, int32_t &max_chunk_z);
size_t empty_world_chunk_count(const octaryn_client_chunk_view &chunk_view);
size_t empty_world_chunk_overlap(const octaryn_client_chunk_view &left,
                                 const octaryn_client_chunk_view &right);
