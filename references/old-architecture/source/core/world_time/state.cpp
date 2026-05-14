#include "core/world_time/internal.h"

#include <cmath>

namespace world_time_internal {

auto clamp_real_seconds_per_day(double value) -> double
{
    if (!(value > 0.0) || !std::isfinite(value))
    {
        return 1800.0;
    }
    return value;
}

auto sanitize_start_seconds_of_day(double value) -> double
{
    if (!std::isfinite(value))
    {
        return 12.0 * 60.0 * 60.0;
    }
    while (value < 0.0)
    {
        value += kWorldSecondsPerDay;
    }
    while (value >= kWorldSecondsPerDay)
    {
        value -= kWorldSecondsPerDay;
    }
    return value;
}

auto sanitize_config(const world_time_config_t* config) -> world_time_config_t
{
    world_time_config_t sanitized = world_time_default_config();
    if (!config)
    {
        return sanitized;
    }
    sanitized.real_seconds_per_day = clamp_real_seconds_per_day(config->real_seconds_per_day);
    if (config->start_month >= 1 && config->start_month <= 12)
    {
        sanitized.start_month = config->start_month;
    }
    sanitized.start_year = config->start_year;
    const int max_day = world_time_days_in_month(sanitized.start_year, sanitized.start_month);
    sanitized.start_day = SDL_clamp(config->start_day, 1, max_day);
    sanitized.start_seconds_of_day = sanitize_start_seconds_of_day(config->start_seconds_of_day);
    return sanitized;
}

auto sanitize_seconds_of_day(double seconds_of_day, Uint64* day_carry) -> double
{
    double sanitized = std::isfinite(seconds_of_day) ? seconds_of_day : 0.0;
    Uint64 carry = 0;
    while (sanitized >= kWorldSecondsPerDay)
    {
        sanitized -= kWorldSecondsPerDay;
        ++carry;
    }
    while (sanitized < 0.0)
    {
        if (carry == 0)
        {
            sanitized = 0.0;
            break;
        }
        sanitized += kWorldSecondsPerDay;
        --carry;
    }
    if (day_carry)
    {
        *day_carry = carry;
    }
    return sanitized;
}

} // namespace world_time_internal

world_time_config_t world_time_default_config(void)
{
    return {
        .real_seconds_per_day = 1800.0,
        .start_year = 1000,
        .start_month = 1,
        .start_day = 1,
        .start_seconds_of_day = 12.0 * 60.0 * 60.0,
    };
}

void world_time_reset(world_time_state_t* state, const world_time_config_t* config)
{
    if (!state)
    {
        return;
    }
    state->config = world_time_internal::sanitize_config(config);
    state->day_index = 0;
    state->seconds_of_day = state->config.start_seconds_of_day;
}

void world_time_advance_real_seconds(world_time_state_t* state, double real_seconds)
{
    if (!state || !std::isfinite(real_seconds) || real_seconds <= 0.0)
    {
        return;
    }

    const double day_scale = world_time_internal::kWorldSecondsPerDay /
                             world_time_internal::clamp_real_seconds_per_day(state->config.real_seconds_per_day);
    double next_seconds = state->seconds_of_day + real_seconds * day_scale;
    while (next_seconds >= world_time_internal::kWorldSecondsPerDay)
    {
        next_seconds -= world_time_internal::kWorldSecondsPerDay;
        ++state->day_index;
    }
    state->seconds_of_day = next_seconds;
}

void world_time_write_blob(const world_time_state_t* state, world_time_blob_t* blob)
{
    if (!blob)
    {
        return;
    }
    blob->version = world_time_internal::kWorldTimeBlobVersion;
    blob->day_index = 0;
    blob->seconds_of_day = 0.0;
    if (!state)
    {
        return;
    }
    blob->day_index = state->day_index;
    blob->seconds_of_day = SDL_clamp(state->seconds_of_day, 0.0, world_time_internal::kWorldSecondsPerDay - 0.001);
}

bool world_time_read_blob(world_time_state_t* state, const world_time_config_t* config, const world_time_blob_t* blob)
{
    if (!state || !blob || blob->version != world_time_internal::kWorldTimeBlobVersion)
    {
        return false;
    }
    world_time_reset(state, config);
    state->day_index = blob->day_index;
    Uint64 day_carry = 0;
    state->seconds_of_day = world_time_internal::sanitize_seconds_of_day(blob->seconds_of_day, &day_carry);
    state->day_index += day_carry;
    return true;
}
