#pragma once

#include <SDL3/SDL.h>

typedef struct world_time_config
{
    double real_seconds_per_day;
    Sint32 start_year;
    Sint32 start_month;
    Sint32 start_day;
    double start_seconds_of_day;
}
world_time_config_t;

typedef struct world_time_date
{
    Sint32 year;
    Sint32 month;
    Sint32 day;
}
world_time_date_t;

typedef struct world_time_state
{
    world_time_config_t config;
    Uint64 day_index;
    double seconds_of_day;
}
world_time_state_t;

typedef struct world_time_snapshot
{
    world_time_date_t date;
    Uint64 day_index;
    Uint32 second_of_day;
    Uint32 hour;
    Uint32 minute;
    Uint32 second;
    double total_world_seconds;
    float day_fraction;
    float solar_elevation;
    float daylight;
    float sunlight;
    float twilight;
    float sky_visibility;
    float gameplay_sky_visibility;
}
world_time_snapshot_t;

typedef struct world_time_blob
{
    Uint32 version;
    Uint64 day_index;
    double seconds_of_day;
}
world_time_blob_t;

world_time_config_t world_time_default_config(void);
void world_time_reset(world_time_state_t* state, const world_time_config_t* config);
void world_time_advance_real_seconds(world_time_state_t* state, double real_seconds);
world_time_snapshot_t world_time_get_snapshot(const world_time_state_t* state);
void world_time_get_celestial_angles(const world_time_snapshot_t* snapshot, float* pitch, float* yaw);
bool world_time_is_leap_year(Sint32 year);
int world_time_days_in_month(Sint32 year, Sint32 month);
void world_time_write_blob(const world_time_state_t* state, world_time_blob_t* blob);
bool world_time_read_blob(world_time_state_t* state, const world_time_config_t* config, const world_time_blob_t* blob);
