#include "app/startup_terrain/terrain.h"

#include "app/runtime/internal.h"
#include "core/log.h"
#include "core/render_distance.h"

void app_prepare_startup_terrain(void)
{
    startup_render_distance = octaryn_sanitize_render_distance(startup_render_distance);

    startup_target_render_distance = startup_render_distance;
    startup_bootstrap_render_distance = startup_render_distance;
    startup_render_distance_bootstrap_pending = false;
    oct_log_infof("Startup terrain | using saved render distance %d chunks", startup_render_distance);
}

int app_render_distance_setting(int live_render_distance)
{
    if (startup_target_render_distance > 0)
    {
        return octaryn_sanitize_render_distance(startup_target_render_distance);
    }
    return octaryn_sanitize_render_distance(live_render_distance);
}

void app_set_render_distance_setting(int requested_render_distance, int live_render_distance)
{
    const int sanitized_request = octaryn_sanitize_render_distance(requested_render_distance);
    const int sanitized_live_distance = octaryn_sanitize_render_distance(live_render_distance);
    startup_render_distance = sanitized_request;
    startup_target_render_distance = sanitized_request;
    startup_bootstrap_render_distance = sanitized_live_distance;
    startup_render_distance_bootstrap_pending = sanitized_live_distance < sanitized_request;
}

void app_reset_startup_terrain_state(void)
{
    spawn_surface_aligned = false;
    spawn_fallback_used = false;
    terrain_debug_logged = false;
    terrain_debug_frame_count = 0;
}
