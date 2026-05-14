#pragma once

#include <FastNoise/FastNoise.h>

#include <array>

#include "world/generation/worldgen.h"
#include "world/runtime/world.h"

namespace worldgen_internal {

constexpr int k_seed = 1337;
constexpr int k_water_height = 30;
struct terrain_noise_set {
    FastNoise::SmartNode<> height;
    FastNoise::SmartNode<> lowland;
    FastNoise::SmartNode<> biome;
    FastNoise::SmartNode<> plant;
};

struct terrain_column {
    int world_x = 0;
    int world_z = 0;
    int local_x = 0;
    int local_z = 0;
    float height = 0.0f;
    int terrain_height = 0;
    int decoration_y = 0;
    block_t top = BLOCK_EMPTY;
    block_t bottom = BLOCK_EMPTY;
    bool low = false;
    bool grass = false;
};

struct terrain_chunk_noise {
    std::array<float, CHUNK_WIDTH * CHUNK_WIDTH> height{};
    std::array<float, CHUNK_WIDTH * CHUNK_WIDTH> lowland{};
    std::array<float, CHUNK_WIDTH * CHUNK_WIDTH> biome{};
    std::array<float, CHUNK_WIDTH * CHUNK_WIDTH> plant{};
};

auto make_noise() -> terrain_noise_set;
void sample_chunk_noise(const terrain_noise_set& noise, int cx, int cz, terrain_chunk_noise* out_noise);
float sample_noise(const FastNoise::SmartNode<>& generator, float x, float z);
void prepare_terrain_column(int world_x,
                            int world_z,
                            int local_x,
                            int local_z,
                            float height_noise,
                            float lowland_noise,
                            float biome_noise,
                            terrain_column* out_column);
void emit_terrain_column_blocks(void* userdata, const terrain_column& column, worldgen_set_block_t function);
void emit_terrain_flora(void* userdata, const terrain_column& column, float plant_noise, worldgen_set_block_t function);

} // namespace worldgen_internal
