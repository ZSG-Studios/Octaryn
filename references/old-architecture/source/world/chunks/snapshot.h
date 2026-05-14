#pragma once

#include <vector>

#include "world/runtime/private.h"

struct neighborhood_snapshot_t
{
    std::vector<block_t> blocks{};
};

auto world_capture_neighborhood_snapshot(chunk_t* neighborhood[3][3], bool capture_blocks) -> neighborhood_snapshot_t;
auto world_snapshot_local_block(const neighborhood_snapshot_t& snapshot,
                                int chunk_x,
                                int chunk_z,
                                int block_x,
                                int block_y,
                                int block_z) -> block_t;
auto world_snapshot_neighborhood_block(const neighborhood_snapshot_t& snapshot,
                                       int bx,
                                       int by,
                                       int bz,
                                       int dx,
                                       int dy,
                                       int dz) -> block_t;
