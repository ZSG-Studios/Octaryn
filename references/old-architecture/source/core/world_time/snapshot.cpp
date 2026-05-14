#include "core/world_time/internal.h"

#include <cmath>

namespace world_time_internal {

auto smoothstep(float edge0, float edge1, float x) -> float
{
    if (edge0 == edge1)
    {
        return x >= edge1 ? 1.0f : 0.0f;
    }
    float t = (x - edge0) / (edge1 - edge0);
    t = SDL_clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

auto wrap_angle(float angle) -> float
{
    while (angle > SDL_PI_F)
    {
        angle -= SDL_PI_F * 2.0f;
    }
    while (angle < -SDL_PI_F)
    {
        angle += SDL_PI_F * 2.0f;
    }
    return angle;
}

auto get_orbit_angle(const world_time_snapshot_t& snapshot) -> float
{
    return snapshot.day_fraction * (SDL_PI_F * 2.0f) - SDL_PI_F * 0.5f;
}

void set_angles_from_orbit(float orbit, float* pitch, float* yaw)
{
    if (pitch)
    {
        *pitch = wrap_angle(-orbit);
    }
    if (yaw)
    {
        *yaw = kEastWestYaw;
    }
}

void set_angles_from_direction(float x, float y, float z, float* pitch, float* yaw)
{
    const float length_sq = x * x + y * y + z * z;
    if (length_sq <= SDL_FLT_EPSILON)
    {
        set_angles_from_orbit(0.0f, pitch, yaw);
        return;
    }

    const float inv_length = 1.0f / SDL_sqrtf(length_sq);
    const float nx = x * inv_length;
    const float ny = y * inv_length;
    const float nz = z * inv_length;

    if (pitch)
    {
        *pitch = SDL_asinf(SDL_clamp(ny, -1.0f, 1.0f));
    }
    if (yaw)
    {
        *yaw = SDL_atan2f(nx, -nz);
    }
}

} // namespace world_time_internal

world_time_snapshot_t world_time_get_snapshot(const world_time_state_t* state)
{
    world_time_state_t fallback = {};
    if (!state)
    {
        world_time_reset(&fallback, NULL);
        state = &fallback;
    }

    world_time_snapshot_t snapshot = {};
    const Uint64 second_of_day = static_cast<Uint64>(SDL_floor(state->seconds_of_day));
    const float day_fraction = static_cast<float>(state->seconds_of_day / world_time_internal::kWorldSecondsPerDay);
    const float orbit = day_fraction * (SDL_PI_F * 2.0f) - SDL_PI_F * 0.5f;
    snapshot.day_index = state->day_index;
    snapshot.second_of_day = static_cast<Uint32>(second_of_day);
    snapshot.hour = static_cast<Uint32>((second_of_day / 3600u) % 24u);
    snapshot.minute = static_cast<Uint32>((second_of_day / 60u) % 60u);
    snapshot.second = static_cast<Uint32>(second_of_day % 60u);
    snapshot.total_world_seconds =
        static_cast<double>(state->day_index) * world_time_internal::kWorldSecondsPerDay + state->seconds_of_day;
    snapshot.day_fraction = day_fraction;
    snapshot.solar_elevation = SDL_sinf(orbit);
    snapshot.daylight = world_time_internal::smoothstep(-0.18f, 0.10f, snapshot.solar_elevation);
    snapshot.sunlight = world_time_internal::smoothstep(-0.02f, 0.18f, snapshot.solar_elevation);
    snapshot.twilight = world_time_internal::smoothstep(-0.28f, 0.08f, snapshot.solar_elevation) *
                        (1.0f - world_time_internal::smoothstep(0.08f, 0.32f, snapshot.solar_elevation));
    snapshot.sky_visibility = SDL_clamp(snapshot.daylight + snapshot.twilight * 0.28f, 0.06f, 1.0f);
    snapshot.gameplay_sky_visibility = SDL_max(snapshot.sky_visibility, 0.42f);

    const world_time_config_t config = world_time_internal::sanitize_config(&state->config);
    const Sint64 start_day = world_time_internal::days_from_civil(
        config.start_year, static_cast<unsigned>(config.start_month), static_cast<unsigned>(config.start_day));
    snapshot.date = world_time_internal::civil_from_days(start_day + static_cast<Sint64>(state->day_index));
    return snapshot;
}

void world_time_get_celestial_angles(const world_time_snapshot_t* snapshot, float* pitch, float* yaw)
{
    const world_time_snapshot_t safe_snapshot = snapshot ? *snapshot : world_time_get_snapshot(NULL);
    world_time_internal::set_angles_from_orbit(world_time_internal::get_orbit_angle(safe_snapshot), pitch, yaw);
}
