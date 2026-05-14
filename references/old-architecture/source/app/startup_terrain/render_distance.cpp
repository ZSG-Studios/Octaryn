#include "app/startup_terrain/terrain.h"

#include "app/runtime/internal.h"
#include "core/log.h"
#include "core/render_distance.h"

void app_maybe_expand_startup_render_distance(void)
{
    if (!startup_render_distance_bootstrap_pending || !terrain_debug_logged)
    {
        return;
    }

    world_debug_stats_t stats{};
    world_get_debug_stats(&stats);
    if (stats.running_jobs > 0)
    {
        return;
    }

    if (world_has_pending_window_transition())
    {
        return;
    }

    const int next_distance =
        octaryn_next_render_distance_step(startup_bootstrap_render_distance, startup_target_render_distance);
    if (next_distance <= startup_bootstrap_render_distance)
    {
        startup_render_distance_bootstrap_pending = false;
        return;
    }

    world_set_render_distance(&player.camera, next_distance);
    if (world_has_pending_window_transition())
    {
        return;
    }
    if (world_get_render_distance() != next_distance)
    {
        return;
    }

    startup_bootstrap_render_distance = next_distance;
    oct_log_infof("Startup terrain | expanded render distance to %d chunks", startup_bootstrap_render_distance);
    if (startup_bootstrap_render_distance >= startup_target_render_distance)
    {
        startup_render_distance_bootstrap_pending = false;
    }
}
