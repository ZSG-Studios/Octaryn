#include "app/world_time/clock.h"

#include "app/runtime/internal.h"
#include "core/log.h"

namespace {

constexpr double kWorldSecondsPerHour = 60.0 * 60.0;
constexpr double kWorldSecondsPerDay = 24.0 * kWorldSecondsPerHour;
constexpr Uint64 kWorldClockPersistIntervalNs = 5ull * SDL_NS_PER_SECOND;

bool g_world_clock_dirty = false;
Uint64 g_last_world_clock_persist_ticks = 0;

} // namespace

void app_persist_world_clock(void)
{
    world_time_blob_t blob = {};
    world_time_write_blob(&world_clock, &blob);
    persistence_set_world_time(&blob, (int) sizeof(blob));
    g_world_clock_dirty = false;
    g_last_world_clock_persist_ticks = SDL_GetTicksNS();
}

void app_mark_world_clock_dirty(void)
{
    g_world_clock_dirty = true;
}

void app_persist_world_clock_if_due(void)
{
    if (!g_world_clock_dirty)
    {
        return;
    }

    const Uint64 now = SDL_GetTicksNS();
    if (g_last_world_clock_persist_ticks != 0 &&
        now - g_last_world_clock_persist_ticks < kWorldClockPersistIntervalNs)
    {
        return;
    }

    app_persist_world_clock();
}

void app_refresh_world_clock_snapshot(void)
{
    world_clock_snapshot = world_time_get_snapshot(&world_clock);
}

void app_init_world_clock(void)
{
    const world_time_config_t config = world_time_default_config();
    world_time_reset(&world_clock, &config);

    persistence_world_save_metadata_t world_save_metadata = {};
    world_time_blob_t blob = {};
    if (persistence_get_world_save_metadata(&world_save_metadata) && world_save_metadata.has_world_time &&
        persistence_get_world_time(&blob, (int) sizeof(blob)))
    {
        world_time_read_blob(&world_clock, &config, &blob);
    }

    app_refresh_world_clock_snapshot();
    app_persist_world_clock();
    oct_log_infof("World clock: %04d-%02d-%02d %02u:%02u:%02u (day=%llu)",
                  world_clock_snapshot.date.year,
                  world_clock_snapshot.date.month,
                  world_clock_snapshot.date.day,
                  world_clock_snapshot.hour,
                  world_clock_snapshot.minute,
                  world_clock_snapshot.second,
                  (unsigned long long) world_clock_snapshot.day_index);
}

void app_step_world_clock_hours(int delta_hours)
{
    if (delta_hours == 0)
    {
        return;
    }

    double next_seconds = world_clock.seconds_of_day + static_cast<double>(delta_hours) * kWorldSecondsPerHour;
    Sint64 day_delta = 0;
    while (next_seconds < 0.0)
    {
        next_seconds += kWorldSecondsPerDay;
        --day_delta;
    }
    while (next_seconds >= kWorldSecondsPerDay)
    {
        next_seconds -= kWorldSecondsPerDay;
        ++day_delta;
    }

    if (day_delta < 0 && world_clock.day_index < static_cast<Uint64>(-day_delta))
    {
        world_clock.day_index = 0;
        world_clock.seconds_of_day = 0.0;
    }
    else
    {
        world_clock.day_index = static_cast<Uint64>(static_cast<Sint64>(world_clock.day_index) + day_delta);
        world_clock.seconds_of_day = next_seconds;
    }

    app_refresh_world_clock_snapshot();
    app_persist_world_clock();
    oct_log_infof("World clock stepped %+d hour(s): %02u:%02u:%02u (day=%llu)",
                  delta_hours,
                  world_clock_snapshot.hour,
                  world_clock_snapshot.minute,
                  world_clock_snapshot.second,
                  (unsigned long long) world_clock_snapshot.day_index);
}
