#include "app/runtime/stream_benchmark.h"

#include <cstdlib>

#include <SDL3/SDL.h>

#include "app/runtime/internal.h"
#include "core/log.h"
#include "core/profile.h"
#include "world/runtime/world.h"

namespace {

bool g_enabled = false;
bool g_configured = false;
bool g_started = false;
bool g_quit_logged = false;
Uint64 g_last_log_ticks = 0;
float g_initial_x = 0.0f;
float g_initial_y = 0.0f;
float g_initial_z = 0.0f;
float g_elapsed_seconds = 0.0f;
float g_raise_blocks = 40.0f;
float g_speed_blocks_per_second = 100.0f;
float g_quit_seconds = 0.0f;
bool g_edit_under_player = false;
bool g_log_edits = false;
float g_edit_interval_ms = 100.0f;
float g_next_edit_seconds = 0.0f;
int g_edit_count = 0;
int g_edit_failures = 0;

bool env_flag_enabled(const char* name)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return false;
    }
    return SDL_strcasecmp(value, "0") != 0 &&
           SDL_strcasecmp(value, "false") != 0 &&
           SDL_strcasecmp(value, "off") != 0 &&
           SDL_strcasecmp(value, "no") != 0;
}

float env_float_value(const char* name, float fallback, float minimum, float maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }

    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value)
    {
        return fallback;
    }
    return SDL_clamp(parsed, minimum, maximum);
}

void maybe_edit_under_player(player_t* player_state)
{
    if (!g_edit_under_player || g_elapsed_seconds < g_next_edit_seconds)
    {
        return;
    }
    OCT_PROFILE_ZONE("stream_benchmark_edit_under_player");
    g_next_edit_seconds = g_elapsed_seconds + g_edit_interval_ms * 0.001f;

    const int world_x = static_cast<int>(SDL_floorf(player_state->camera.position[0]));
    const int world_z = static_cast<int>(SDL_floorf(player_state->camera.position[2]));
    int surface_y = 0;
    block_t surface_block = BLOCK_EMPTY;
    if (!world_try_get_surface_height(world_x, world_z, &surface_y, &surface_block) || surface_block == BLOCK_EMPTY)
    {
        ++g_edit_failures;
        return;
    }

    int edit_position[3] = {world_x, surface_y, world_z};
    if (world_queue_block_edit(edit_position, BLOCK_EMPTY))
    {
        ++g_edit_count;
        if (g_log_edits)
        {
            oct_log_infof("Stream benchmark edit: count=%d pos=(%d,%d,%d) old=%d failures=%d",
                          g_edit_count,
                          edit_position[0],
                          edit_position[1],
                          edit_position[2],
                          static_cast<int>(surface_block),
                          g_edit_failures);
        }
        return;
    }

    ++g_edit_failures;
}

} // namespace

void app_stream_benchmark_init(void)
{
    if (g_configured)
    {
        return;
    }
    g_configured = true;
    g_enabled = env_flag_enabled("OCTARYN_STREAM_BENCH");
    if (!g_enabled)
    {
        return;
    }

    g_raise_blocks = env_float_value("OCTARYN_STREAM_BENCH_RAISE", 40.0f, 0.0f, 512.0f);
    g_speed_blocks_per_second = env_float_value("OCTARYN_STREAM_BENCH_SPEED", 100.0f, 1.0f, 2048.0f);
    g_quit_seconds = env_float_value("OCTARYN_STREAM_BENCH_QUIT_SECONDS", 0.0f, 0.0f, 3600.0f);
    g_edit_under_player = env_flag_enabled("OCTARYN_STREAM_BENCH_EDIT_UNDER_PLAYER");
    g_log_edits = env_flag_enabled("OCTARYN_STREAM_BENCH_LOG_EDITS");
    g_edit_interval_ms = env_float_value("OCTARYN_STREAM_BENCH_EDIT_INTERVAL_MS", 100.0f, 16.0f, 2000.0f);
    g_next_edit_seconds = 0.0f;
    g_edit_count = 0;
    g_edit_failures = 0;
    oct_log_infof("Stream benchmark enabled: raise=%.2f speed=%.2f quit_seconds=%.2f direction=+X edit_under_player=%d edit_interval_ms=%.2f",
                  g_raise_blocks,
                  g_speed_blocks_per_second,
                  g_quit_seconds,
                  g_edit_under_player ? 1 : 0,
                  g_edit_interval_ms);
}

bool app_stream_benchmark_active(void)
{
    return g_enabled;
}

void app_stream_benchmark_step(player_t* player_state, float dt)
{
    if (!g_enabled || !player_state || !spawn_surface_aligned)
    {
        return;
    }
    OCT_PROFILE_ZONE("stream_benchmark_step");

    if (!g_started)
    {
        g_started = true;
        g_initial_x = player_state->camera.position[0];
        g_initial_y = player_state->camera.position[1];
        g_initial_z = player_state->camera.position[2];
        player_state->controller = PLAYER_CONTROLLER_FLY;
        player_state->velocity[0] = 0.0f;
        player_state->velocity[1] = 0.0f;
        player_state->velocity[2] = 0.0f;
        player_state->camera.pitch = 0.0f;
        player_state->camera.yaw = 90.0f;
        oct_log_infof("Stream benchmark start: base=(%.2f, %.2f, %.2f)",
                      g_initial_x,
                      g_initial_y,
                      g_initial_z);
    }

    const float seconds = SDL_min(dt * 0.001f, 0.25f);
    g_elapsed_seconds += seconds;
    player_state->controller = PLAYER_CONTROLLER_FLY;
    player_state->velocity[0] = 0.0f;
    player_state->velocity[1] = 0.0f;
    player_state->velocity[2] = 0.0f;
    player_state->camera.position[0] += g_speed_blocks_per_second * seconds;
    player_state->camera.position[1] = g_initial_y + g_raise_blocks;
    player_state->camera.position[2] = g_initial_z;
    player_state->camera.pitch = 0.0f;
    player_state->camera.yaw = 90.0f;
    maybe_edit_under_player(player_state);

    const Uint64 now_ticks = SDL_GetTicksNS();
    if (g_last_log_ticks == 0u || now_ticks - g_last_log_ticks >= 2000000000ull)
    {
        world_debug_stats_t stats = {};
        world_get_debug_stats(&stats);
        world_edit_debug_stats_t edit_stats = {};
        world_get_edit_debug_stats(&edit_stats);
        const app_frame_metrics_snapshot_t frame_metrics = main_frame_profile_metrics_snapshot(&frame_profile);
        camera_update(&player_state->camera);
        const int visible_mesh_chunks = world_count_visible_mesh_chunks(&player_state->camera);
        oct_log_infof("Stream benchmark: t=%.2f pos=(%.2f, %.2f, %.2f) loaded=%d meshed=%d visible=%d running_jobs=%u dirty_pooled_slots=%u edits=%d edit_failures=%d edit_queued=%llu edit_applied=%llu edit_changed=%llu edit_pending=%u edit_deferred=%llu current_ms=%.2f current_fps=%.1f avg_ms=%.2f avg_fps=%.1f low_1pct_ms=%.2f low_1pct_fps=%.1f low_0_1pct_ms=%.2f low_0_1pct_fps=%.1f low_x5_ms=%.2f low_x5_fps=%.1f low_x10_ms=%.2f low_x10_fps=%.1f worst_ms=%.2f worst_fps=%.1f metric_samples=%llu warmup=%u",
                      g_elapsed_seconds,
                      player_state->camera.position[0],
                      player_state->camera.position[1],
                      player_state->camera.position[2],
                      stats.loaded_chunks,
                      stats.mesh_ready_chunks,
                      visible_mesh_chunks,
                      stats.running_jobs,
                      stats.dirty_pooled_chunk_slots,
                      g_edit_count,
                      g_edit_failures,
                      static_cast<unsigned long long>(edit_stats.queued),
                      static_cast<unsigned long long>(edit_stats.applied),
                      static_cast<unsigned long long>(edit_stats.changed),
                      edit_stats.pending,
                      static_cast<unsigned long long>(edit_stats.deferred_attempts),
                      frame_metrics.current.ms,
                      frame_metrics.current.fps,
                      frame_metrics.average.ms,
                      frame_metrics.average.fps,
                      frame_metrics.low_1pct.ms,
                      frame_metrics.low_1pct.fps,
                      frame_metrics.low_0_1pct.ms,
                      frame_metrics.low_0_1pct.fps,
                      frame_metrics.confirmed_low_5.ms,
                      frame_metrics.confirmed_low_5.fps,
                      frame_metrics.confirmed_low_10.ms,
                      frame_metrics.confirmed_low_10.fps,
                      frame_metrics.worst.ms,
                      frame_metrics.worst.fps,
                      static_cast<unsigned long long>(frame_metrics.sample_count),
                      frame_metrics.warmup_complete ? 1u : 0u);
        g_last_log_ticks = now_ticks;
    }

    if (g_quit_seconds > 0.0f && g_elapsed_seconds >= g_quit_seconds && !g_quit_logged)
    {
        g_quit_logged = true;
        quit_requested = true;
        oct_log_infof("Stream benchmark requested shutdown: t=%.2f quit_seconds=%.2f",
                      g_elapsed_seconds,
                      g_quit_seconds);
    }
}
