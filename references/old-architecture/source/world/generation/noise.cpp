#include "world/generation/internal.h"

namespace {

auto make_fbm_noise(float frequency, int octaves, int seed_offset) -> FastNoise::SmartNode<>
{
    auto source = FastNoise::New<FastNoise::Perlin>();
    source->SetScale(1.0f / frequency);
    source->SetSeedOffset(seed_offset);

    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(source);
    fractal->SetGain(0.5f);
    fractal->SetOctaveCount(octaves);
    fractal->SetLacunarity(2.0f);
    return fractal;
}

} // namespace

namespace worldgen_internal {

auto make_noise() -> terrain_noise_set
{
    return {
        .height = make_fbm_noise(0.005f, 6, 0),
        .lowland = make_fbm_noise(0.01f, 6, 101),
        .biome = make_fbm_noise(0.2f, 6, 211),
        .plant = make_fbm_noise(0.2f, 3, 307),
    };
}

void sample_chunk_noise(const terrain_noise_set& noise, int cx, int cz, terrain_chunk_noise* out_noise)
{
    if (!out_noise)
    {
        return;
    }

    noise.height->GenUniformGrid2D(out_noise->height.data(),
                                   static_cast<float>(cx),
                                   static_cast<float>(cz),
                                   CHUNK_WIDTH,
                                   CHUNK_WIDTH,
                                   1.0f,
                                   1.0f,
                                   k_seed);
    noise.lowland->GenUniformGrid2D(out_noise->lowland.data(),
                                    static_cast<float>(-cx),
                                    static_cast<float>(cz),
                                    CHUNK_WIDTH,
                                    CHUNK_WIDTH,
                                    -1.0f,
                                    1.0f,
                                    k_seed);
    noise.biome->GenUniformGrid2D(out_noise->biome.data(),
                                  static_cast<float>(cx),
                                  static_cast<float>(cz),
                                  CHUNK_WIDTH,
                                  CHUNK_WIDTH,
                                  1.0f,
                                  1.0f,
                                  k_seed);
    noise.plant->GenUniformGrid2D(out_noise->plant.data(),
                                  static_cast<float>(cx),
                                  static_cast<float>(cz),
                                  CHUNK_WIDTH,
                                  CHUNK_WIDTH,
                                  1.0f,
                                  1.0f,
                                  k_seed);
}

float sample_noise(const FastNoise::SmartNode<>& generator, float x, float z)
{
    return generator->GenSingle2D(x, z, k_seed);
}

} // namespace worldgen_internal
