#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "core/profile.h"
#include "core/render_distance.h"
#include "world/runtime/world.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"
#include "world/runtime/update_telemetry.h"
#include "world/window/internal.h"

namespace {

typedef enum world_window_transition_kind
{
    WORLD_WINDOW_TRANSITION_NONE,
    WORLD_WINDOW_TRANSITION_MOVE,
    WORLD_WINDOW_TRANSITION_RENDER_DISTANCE,
    WORLD_WINDOW_TRANSITION_RESET,
}
world_window_transition_kind_t;

world_window_transition_kind_t g_pending_transition_kind = WORLD_WINDOW_TRANSITION_NONE;
int g_pending_transition_origin_x = 0;
int g_pending_transition_origin_z = 0;
int g_pending_transition_active_world_width = 0;

bool pending_transition_is_explicit()
{
    return g_pending_transition_kind == WORLD_WINDOW_TRANSITION_RENDER_DISTANCE ||
           g_pending_transition_kind == WORLD_WINDOW_TRANSITION_RESET;
}

void queue_window_transition(world_window_transition_kind_t kind,
                             int origin_x,
                             int origin_z,
                             int active_world_width)
{
    g_pending_transition_kind = kind;
    g_pending_transition_origin_x = origin_x;
    g_pending_transition_origin_z = origin_z;
    g_pending_transition_active_world_width = active_world_width;
    world_set_is_moving_internal(kind != WORLD_WINDOW_TRANSITION_NONE);
}

} // namespace

bool world_window_transition_pending_internal(void)
{
    return g_pending_transition_kind != WORLD_WINDOW_TRANSITION_NONE;
}

bool world_has_pending_window_transition(void)
{
    return world_window_transition_pending_internal();
}

void world_window_queue_move_internal(int origin_x, int origin_z)
{
    if (pending_transition_is_explicit())
    {
        return;
    }
    queue_window_transition(WORLD_WINDOW_TRANSITION_MOVE, origin_x, origin_z, world_active_world_width_internal());
}

void world_window_queue_render_distance_internal(const camera_t* camera, int chunk_distance)
{
    const int sanitized_distance = octaryn_sanitize_render_distance(chunk_distance);
    const int active_world_width = SDL_clamp(sanitized_distance + 2, 4, MAX_WORLD_WIDTH);
    const int origin_x = world_window_origin_for_position_with_width(camera->position[0], active_world_width);
    const int origin_z = world_window_origin_for_position_with_width(camera->position[2], active_world_width);
    if (!world_window_transition_pending_internal() &&
        active_world_width == world_active_world_width_internal() &&
        origin_x == world_origin_x_internal() &&
        origin_z == world_origin_z_internal())
    {
        return;
    }
    queue_window_transition(WORLD_WINDOW_TRANSITION_RENDER_DISTANCE, origin_x, origin_z, active_world_width);
}

void world_window_queue_reset_internal(const camera_t* camera)
{
    const int origin_x = world_window_origin_for_position(camera->position[0]);
    const int origin_z = world_window_origin_for_position(camera->position[2]);
    queue_window_transition(WORLD_WINDOW_TRANSITION_RESET,
                            origin_x,
                            origin_z,
                            world_active_world_width_internal());
}

bool world_window_try_apply_pending_transition_internal(void)
{
    if (!world_window_transition_pending_internal() || world_jobs_cpu_running_count() > 0)
    {
        return false;
    }

    const world_window_transition_kind_t kind = g_pending_transition_kind;
    const int origin_x = g_pending_transition_origin_x;
    const int origin_z = g_pending_transition_origin_z;
    const int active_world_width = g_pending_transition_active_world_width;

    g_pending_transition_kind = WORLD_WINDOW_TRANSITION_NONE;
    g_pending_transition_origin_x = 0;
    g_pending_transition_origin_z = 0;
    g_pending_transition_active_world_width = 0;

    switch (kind)
    {
    case WORLD_WINDOW_TRANSITION_MOVE:
    case WORLD_WINDOW_TRANSITION_RENDER_DISTANCE:
        world_window_reframe_internal(origin_x, origin_z, active_world_width);
        world_set_is_moving_internal(false);
        return true;
    case WORLD_WINDOW_TRANSITION_RESET:
        world_window_reset_at_origin_internal(origin_x, origin_z);
        world_set_is_moving_internal(false);
        return true;
    case WORLD_WINDOW_TRANSITION_NONE:
    default:
        world_set_is_moving_internal(false);
        return false;
    }
}

void world_set_render_distance(const camera_t* camera, int chunk_distance)
{
    world_window_queue_render_distance_internal(camera, chunk_distance);
    (void) world_window_try_apply_pending_transition_internal();
}

void world_reset_window(const camera_t* camera)
{
    world_window_queue_reset_internal(camera);
    (void) world_window_try_apply_pending_transition_internal();
}

void world_update(const camera_t* camera)
{
    const Uint64 update_start = oct_profile_now_ticks();
    Uint64 step_start = oct_profile_now_ticks();
    world_apply_pending_block_edits();
    world_update_telemetry_record_edit(oct_profile_elapsed_ms(step_start));

    step_start = oct_profile_now_ticks();
    world_window_move_chunks_internal(camera);
    world_update_telemetry_record_move(oct_profile_elapsed_ms(step_start));
    if (world_window_transition_pending_internal() || world_is_moving_internal())
    {
        step_start = oct_profile_now_ticks();
        (void) world_window_try_apply_pending_transition_internal();
        world_update_telemetry_record_transition(oct_profile_elapsed_ms(step_start));
    }
    step_start = oct_profile_now_ticks();
    world_jobs_update(camera, !world_window_transition_pending_internal());
    world_update_telemetry_record_jobs(oct_profile_elapsed_ms(step_start));
    world_update_telemetry_record_update(oct_profile_elapsed_ms(update_start));
}
