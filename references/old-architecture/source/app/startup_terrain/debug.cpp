#include "app/startup_terrain/terrain.h"

#include "app/runtime/internal.h"
#include "core/env.h"
#include "core/log.h"

namespace {

bool startup_terrain_diagnostics_enabled()
{
    return oct_env_flag_enabled("OCTARYN_LOG_STARTUP_TERRAIN") || oct_env_flag_enabled("OCTARYN_LOG_WORLD_TIMING") ||
        oct_env_flag_enabled("OCTARYN_LOG_RENDER_PROFILE");
}

} // namespace

void app_log_world_debug_snapshot(const char* reason)
{
    camera_update(&player.camera);
    world_debug_stats_t stats{};
    world_get_debug_stats(&stats);
    const int visible_mesh_chunks = world_count_visible_mesh_chunks(&player.camera);
    const int visible_missing_skylight_chunks = world_count_visible_chunks_missing_skylight(&player.camera);
    const int column_x = static_cast<int>(SDL_floorf(player.camera.position[0]));
    const int column_z = static_cast<int>(SDL_floorf(player.camera.position[2]));
    int surface_y = -1;
    block_t surface_block = BLOCK_EMPTY;
    const bool has_surface = world_try_get_surface_height(column_x, column_z, &surface_y, &surface_block);
    oct_log_infof(
        "Terrain debug (%s): camera=(%.1f, %.1f, %.1f) loaded=%d/%d meshed=%d visible=%d visible_missing_skylight=%d running_jobs=%d opaque_chunks=%d opaque_vertices=%u transparent_chunks=%d sprite_chunks=%d surface=%s%d block=%d query=%d pitch=%.2f yaw=%.2f",
        reason,
        player.camera.position[0],
        player.camera.position[1],
        player.camera.position[2],
        stats.loaded_chunks,
        stats.active_chunks,
        stats.mesh_ready_chunks,
        visible_mesh_chunks,
        visible_missing_skylight_chunks,
        stats.running_jobs,
        stats.chunks_with_opaque_mesh,
        stats.opaque_vertices,
        stats.chunks_with_transparent_mesh,
        stats.chunks_with_sprite_mesh,
        has_surface ? "" : "none:",
        has_surface ? surface_y : -1,
        has_surface ? surface_block : BLOCK_EMPTY,
        player.query.block,
        player.camera.pitch,
        player.camera.yaw);
}

void app_maybe_log_terrain_debug(void)
{
    if (terrain_debug_logged)
    {
        return;
    }

    world_debug_stats_t stats{};
    world_get_debug_stats(&stats);
    camera_update(&player.camera);
    const int visible_mesh_chunks = world_count_visible_mesh_chunks(&player.camera);
    const float elapsed_ms = static_cast<float>(SDL_GetTicksNS() - startup_ticks_ns) * 1e-6f;
    if (stats.mesh_ready_chunks > 0)
    {
        if (visible_mesh_chunks == 0 && player_loaded_from_save && !spawn_fallback_used)
        {
            if (stats.running_jobs == 0)
            {
                app_fallback_to_default_spawn("meshes uploaded but no chunks are visible from saved camera");
            }
            return;
        }
        if (startup_terrain_diagnostics_enabled())
        {
            app_log_world_debug_snapshot("meshes-ready");
        }
        terrain_debug_logged = true;
        return;
    }

    terrain_debug_frame_count++;
    if (startup_terrain_diagnostics_enabled() && (terrain_debug_frame_count % 30) == 0)
    {
        oct_log_infof("Terrain wait | frame=%d elapsed=%.2f ms loaded=%d/%d meshed=%d visible=%d running_jobs=%d",
                      terrain_debug_frame_count,
                      elapsed_ms,
                      stats.loaded_chunks,
                      stats.active_chunks,
                      stats.mesh_ready_chunks,
                      visible_mesh_chunks,
                      stats.running_jobs);
    }
    if (elapsed_ms >= 3000.0f)
    {
        if (player_loaded_from_save && !spawn_fallback_used && stats.running_jobs == 0)
        {
            app_fallback_to_default_spawn("saved camera did not reach a visible terrain chunk within 3 seconds");
            return;
        }
        if (startup_terrain_diagnostics_enabled() && (terrain_debug_frame_count % 120) == 0)
        {
            app_log_world_debug_snapshot("waiting-for-meshes");
        }
    }
}
