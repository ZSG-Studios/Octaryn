#include "app/startup_terrain/terrain.h"

#include "app/runtime/internal.h"
#include "core/log.h"

float app_spawn_eye_height(void)
{
    return 1.0f + 1.62f + 0.1f;
}

void app_fallback_to_default_spawn(const char* reason)
{
    player_reset_spawn(&player);
    player_loaded_from_save = false;
    spawn_surface_aligned = false;
    spawn_fallback_used = true;
    terrain_debug_frame_count = 0;
    world_reset_window(&player.camera);
    player_update_query(&player);
    oct_log_warnf("Saved spawn fallback triggered: %s; resetting to default spawn at (0, 80, 0)", reason);
}

void app_maybe_align_spawn_to_surface(void)
{
    if (spawn_surface_aligned || world_has_pending_window_transition())
    {
        return;
    }

    const int column_x = static_cast<int>(SDL_floorf(player.camera.position[0]));
    const int column_z = static_cast<int>(SDL_floorf(player.camera.position[2]));
    int surface_y = 0;
    block_t surface_block = BLOCK_EMPTY;
    if (!world_try_get_surface_height(column_x, column_z, &surface_y, &surface_block))
    {
        return;
    }

    const float desired_eye_y = static_cast<float>(surface_y) + app_spawn_eye_height();
    const bool should_adjust = !player_loaded_from_save ||
        player.camera.position[1] < desired_eye_y ||
        player.camera.position[1] > desired_eye_y + 24.0f;
    if (should_adjust)
    {
        const float previous_y = player.camera.position[1];
        player.camera.position[1] = desired_eye_y;
        player.velocity[0] = 0.0f;
        player.velocity[1] = 0.0f;
        player.velocity[2] = 0.0f;
        player.is_on_ground = false;
        if (!player_loaded_from_save)
        {
            player.camera.pitch = -0.35f;
        }
        oct_log_infof(
            "Spawn aligned to terrain at column (%d, %d): surface_y=%d block=%d eye_y %.2f -> %.2f%s",
            column_x,
            column_z,
            surface_y,
            surface_block,
            previous_y,
            player.camera.position[1],
            player_loaded_from_save ? " (adjusted saved position)" : "");
    }
    else
    {
        oct_log_infof(
            "Spawn kept near terrain at column (%d, %d): surface_y=%d block=%d eye_y=%.2f%s",
            column_x,
            column_z,
            surface_y,
            surface_block,
            player.camera.position[1],
            player_loaded_from_save ? " (loaded from save)" : "");
    }

    spawn_surface_aligned = true;
}
