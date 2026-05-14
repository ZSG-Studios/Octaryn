#include "world/generation/internal.h"

#include <SDL3/SDL.h>

#include "world/runtime/world.h"

namespace {

struct terrain_materials {
    block_t top;
    block_t bottom;
    bool grass;
};

float compute_height(float height_noise, float lowland_noise, bool* out_low)
{
    float height = height_noise * 50.0f;
    height = SDL_powf(SDL_max(height, 0.0f), 1.3f);
    height += 30.0f;
    height = SDL_clamp(height, 0.0f, static_cast<float>(CHUNK_HEIGHT - 1));
    if (height < 40.0f)
    {
        height += lowland_noise * 12.0f;
        *out_low = true;
    }
    return height;
}

auto classify_materials(float height, float biome) -> terrain_materials
{
    if (height + biome < 31.0f)
    {
        return {
            .top = BLOCK_SAND,
            .bottom = BLOCK_SAND,
            .grass = false,
        };
    }

    biome *= 8.0f;
    biome = SDL_clamp(biome, -5.0f, 5.0f);
    if (height + biome < 61.0f)
    {
        return {
            .top = BLOCK_GRASS,
            .bottom = BLOCK_DIRT,
            .grass = true,
        };
    }
    if (height + biome < 132.0f)
    {
        return {
            .top = BLOCK_STONE,
            .bottom = BLOCK_STONE,
            .grass = false,
        };
    }
    return {
        .top = BLOCK_SNOW,
        .bottom = BLOCK_STONE,
        .grass = false,
    };
}

} // namespace

namespace worldgen_internal {

void prepare_terrain_column(int world_x,
                            int world_z,
                            int local_x,
                            int local_z,
                            float height_noise,
                            float lowland_noise,
                            float biome_noise,
                            terrain_column* out_column)
{
    bool low = false;
    const float height = compute_height(height_noise, lowland_noise, &low);
    const terrain_materials materials = classify_materials(height, biome_noise);

    out_column->world_x = world_x;
    out_column->world_z = world_z;
    out_column->local_x = local_x;
    out_column->local_z = local_z;
    out_column->height = height;
    out_column->terrain_height = static_cast<int>(SDL_ceilf(height));
    out_column->decoration_y = SDL_max(out_column->terrain_height, k_water_height);
    out_column->top = materials.top;
    out_column->bottom = materials.bottom;
    out_column->low = low;
    out_column->grass = materials.grass;
}

void emit_terrain_column_blocks(void* userdata, const terrain_column& column, worldgen_set_block_t function)
{
    int y = 0;
    for (; y < column.terrain_height; y++)
    {
        function(userdata, column.world_x, y, column.world_z, column.bottom);
    }
    function(userdata, column.world_x, y, column.world_z, column.top);
    for (; y < k_water_height; y++)
    {
        function(userdata, column.world_x, y, column.world_z, BLOCK_WATER);
    }
}

} // namespace worldgen_internal
