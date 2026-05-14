#pragma once

#include "app/managed/managed_abi.h"

typedef struct player player_t;
typedef struct world_time_snapshot world_time_snapshot_t;

oct_managed_frame_snapshot_t app_managed_frame_snapshot_build(const player_t* player,
                                                              const world_time_snapshot_t* world_time,
                                                              double delta_seconds,
                                                              std::uint64_t frame_index);
