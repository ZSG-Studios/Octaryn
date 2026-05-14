#include <SDL3/SDL.h>

#include <atomic>

#include "core/env.h"
#include "core/log.h"
#include "core/profile.h"
#include "world/runtime/world.h"
#include "world/generation/internal.h"

namespace {

std::atomic_int g_worldgen_profile_logs{0};

bool worldgen_profile_logging_enabled()
{
    return oct_env_flag_enabled("OCTARYN_LOG_WORLD_TIMING") || oct_env_flag_enabled("OCTARYN_LOG_RENDER_PROFILE");
}

} // namespace

void worldgen_get_blocks(void* userdata, int cx, int cz, worldgen_set_block_t function)
{
    OCT_PROFILE_ZONE("worldgen_get_blocks");
    const Uint64 start_ticks = oct_profile_now_ticks();
    thread_local const worldgen_internal::terrain_noise_set noise = worldgen_internal::make_noise();
    worldgen_internal::terrain_chunk_noise chunk_noise{};
    worldgen_internal::sample_chunk_noise(noise, cx, cz, &chunk_noise);
    for (int a = 0; a < CHUNK_WIDTH; a++)
    for (int b = 0; b < CHUNK_WIDTH; b++)
    {
        const std::size_t noise_index = static_cast<std::size_t>(b * CHUNK_WIDTH + a);
        worldgen_internal::terrain_column column{};
        worldgen_internal::prepare_terrain_column(cx + a,
                                                  cz + b,
                                                  a,
                                                  b,
                                                  chunk_noise.height[noise_index],
                                                  chunk_noise.lowland[noise_index],
                                                  chunk_noise.biome[noise_index],
                                                  &column);
        worldgen_internal::emit_terrain_column_blocks(userdata, column, function);
        worldgen_internal::emit_terrain_flora(userdata, column, chunk_noise.plant[noise_index], function);
    }
    const float elapsed_ms = oct_profile_elapsed_ms(start_ticks);
    if (worldgen_profile_logging_enabled() && elapsed_ms >= 8.0f)
    {
        const int log_index = g_worldgen_profile_logs.fetch_add(1);
        if (log_index < 24)
        {
            oct_log_infof("Terrain timing | worldgen chunk (%d, %d) took %.2f ms", cx, cz, elapsed_ms);
        }
    }
}
