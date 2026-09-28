#pragma once
#include <stdint.h>

typedef struct octaryn_remote_command {
    uint64_t frame_index;
    uint32_t flags, controller;
    float move_x, move_y, move_z, pitch, yaw;
    int32_t relative_mouse;
} octaryn_remote_command;

typedef struct octaryn_remote_pose {
    uint32_t version, size;
    uint64_t acknowledged_input_frame, source_tick;
    double source_seconds, world_total_seconds;
    float x, y, z, pitch, yaw, velocity_x, velocity_y, velocity_z, world_day_fraction;
    uint32_t flags;
} octaryn_remote_pose;
