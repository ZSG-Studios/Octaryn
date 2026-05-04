#pragma once

#include "octaryn_client_native_empty_world_mesh.h"

#include <cstddef>
#include <cstdint>

bool native_empty_world_chunk_range(
    const octaryn_client_chunk_view &chunk_view, int32_t &min_chunk_x,
    int32_t &max_chunk_x, int32_t &min_chunk_z, int32_t &max_chunk_z);
size_t native_empty_world_chunk_count(
    const octaryn_client_chunk_view &chunk_view);
size_t native_empty_world_chunk_overlap(
    const octaryn_client_chunk_view &left,
    const octaryn_client_chunk_view &right);
