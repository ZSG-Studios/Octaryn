#include "world/generation/internal.h"

#include <SDL3/SDL.h>

#include "world/runtime/world.h"

namespace worldgen_internal {

void emit_terrain_flora(void* userdata, const terrain_column& column, float plant_noise, worldgen_set_block_t function)
{
    if (!(column.low && column.grass))
    {
        return;
    }

    const float plant = plant_noise * 0.5f + 0.5f;
    if (plant > 0.8f && column.local_x > 2 && column.local_x < CHUNK_WIDTH - 2 && column.local_z > 2 && column.local_z < CHUNK_WIDTH - 2)
    {
        const int log = static_cast<int>(3.0f + plant * 2.0f);
        for (int dy = 0; dy < log; dy++)
        {
            function(userdata, column.world_x, column.decoration_y + dy + 1, column.world_z, BLOCK_LOG);
        }
        for (int dx = -1; dx <= 1; dx++)
        for (int dz = -1; dz <= 1; dz++)
        for (int dy = 0; dy < 2; dy++)
        {
            if (dx || dz || dy)
            {
                function(userdata, column.world_x + dx, column.decoration_y + log + dy, column.world_z + dz, BLOCK_LEAVES);
            }
        }
        return;
    }

    if (plant > 0.55f)
    {
        function(userdata, column.world_x, column.decoration_y + 1, column.world_z, BLOCK_BUSH);
        return;
    }

    if (plant > 0.52f)
    {
        const int value = SDL_max(static_cast<int>(plant * 1000.0f) % 4, 0);
        const block_t flowers[] = {BLOCK_BLUEBELL, BLOCK_GARDENIA, BLOCK_LAVENDER, BLOCK_ROSE};
        function(userdata, column.world_x, column.decoration_y + 1, column.world_z, flowers[value]);
    }
}

} // namespace worldgen_internal
