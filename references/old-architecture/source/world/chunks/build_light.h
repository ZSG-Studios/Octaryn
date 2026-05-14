#pragma once

#include "world/chunks/snapshot.h"

void world_chunk_build_lights(const neighborhood_snapshot_t& snapshot, cpu_buffer_t* skylight, Uint32 dirty_flags);
