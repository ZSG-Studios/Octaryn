#pragma once

#include "core/world_time/time.h"

#include <SDL3/SDL.h>

namespace world_time_internal {

inline constexpr double kWorldSecondsPerDay = 24.0 * 60.0 * 60.0;
inline constexpr Uint32 kWorldTimeBlobVersion = 1;
inline constexpr float kEastWestYaw = -SDL_PI_F * 0.5f;

auto clamp_real_seconds_per_day(double value) -> double;
auto sanitize_start_seconds_of_day(double value) -> double;
auto sanitize_config(const world_time_config_t* config) -> world_time_config_t;
auto sanitize_seconds_of_day(double seconds_of_day, Uint64* day_carry) -> double;

auto smoothstep(float edge0, float edge1, float x) -> float;
auto wrap_angle(float angle) -> float;
auto get_orbit_angle(const world_time_snapshot_t& snapshot) -> float;
void set_angles_from_orbit(float orbit, float* pitch, float* yaw);
void set_angles_from_direction(float x, float y, float z, float* pitch, float* yaw);

auto days_from_civil(Sint32 year, unsigned month, unsigned day) -> Sint64;
auto civil_from_days(Sint64 z) -> world_time_date_t;

} // namespace world_time_internal
