#include "world/lighting/skylight_cpu.h"

#include <algorithm>
#include <array>
#include <vector>

namespace skylight_cpu {

namespace {

struct skylight_node_t
{
    int x;
    int y;
    int z;
};

constexpr std::array kLateralOffsets = {
    skylight_node_t{1, 0, 0},
    skylight_node_t{-1, 0, 0},
    skylight_node_t{0, 0, 1},
    skylight_node_t{0, 0, -1},
};

constexpr std::array kPropagationOffsets = {
    skylight_node_t{1, 0, 0},
    skylight_node_t{-1, 0, 0},
    skylight_node_t{0, 1, 0},
    skylight_node_t{0, -1, 0},
    skylight_node_t{0, 0, 1},
    skylight_node_t{0, 0, -1},
};

auto neighborhood_index(int x, int y, int z) -> std::size_t
{
    return static_cast<std::size_t>((x * CHUNK_HEIGHT + y) * kNeighborhoodDepth + z);
}

auto sample_block(const block_t* neighborhood_blocks, int x, int y, int z) -> block_t
{
    return neighborhood_blocks[neighborhood_index(x, y, z)];
}

auto skylight_opacity(block_t block) -> Uint8
{
    return static_cast<Uint8>(block_get_skylight_opacity(block));
}

auto can_receive_lateral_skylight(block_t block) -> bool
{
    return skylight_opacity(block) < kMaxValue;
}

auto neighborhood_value_count() -> std::size_t
{
    return static_cast<std::size_t>(kNeighborhoodWidth) * CHUNK_HEIGHT * kNeighborhoodDepth;
}

auto propagated_skylight(Uint8 current, block_t block) -> Uint8
{
    if (!can_receive_lateral_skylight(block) || current <= 1)
    {
        return 0;
    }

    const Uint8 opacity = skylight_opacity(block);
    const Uint8 attenuation = std::max<Uint8>(1, opacity);
    return current > attenuation ? static_cast<Uint8>(current - attenuation) : static_cast<Uint8>(0);
}

auto neighbor_can_receive_more_skylight(const block_t* neighborhood_blocks,
                                        const Uint8* neighborhood_values,
                                        Uint8 current,
                                        int nx,
                                        int ny,
                                        int nz) -> bool
{
    if (nx < 0 || nx >= kNeighborhoodWidth || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= kNeighborhoodDepth)
    {
        return false;
    }

    const Uint8 propagated = propagated_skylight(current, sample_block(neighborhood_blocks, nx, ny, nz));
    return propagated > neighborhood_values[neighborhood_index(nx, ny, nz)];
}

void seed_direct_skylight(const block_t* neighborhood_blocks, Uint8* neighborhood_values)
{
    for (int x = 0; x < kNeighborhoodWidth; ++x)
    {
        for (int z = 0; z < kNeighborhoodDepth; ++z)
        {
            Uint8 skylight = kMaxValue;
            for (int y = CHUNK_HEIGHT - 1; y >= 0 && skylight > 0; --y)
            {
                const block_t block = sample_block(neighborhood_blocks, x, y, z);
                const Uint8 opacity = skylight_opacity(block);
                if (opacity >= kMaxValue)
                {
                    break;
                }
                if (opacity > 0)
                {
                    skylight = skylight > opacity ? static_cast<Uint8>(skylight - opacity) : static_cast<Uint8>(0);
                }
                if (skylight == 0)
                {
                    break;
                }
                neighborhood_values[neighborhood_index(x, y, z)] = skylight;
            }
        }
    }
}

void seed_lateral_frontier(const block_t* neighborhood_blocks,
                           const Uint8* neighborhood_values,
                           std::vector<skylight_node_t>* frontier)
{
    for (int x = 0; x < kNeighborhoodWidth; ++x)
    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    for (int z = 0; z < kNeighborhoodDepth; ++z)
    {
        const Uint8 current = neighborhood_values[neighborhood_index(x, y, z)];
        if (current <= 1)
        {
            continue;
        }
        for (const skylight_node_t offset : kLateralOffsets)
        {
            if (neighbor_can_receive_more_skylight(neighborhood_blocks,
                                                   neighborhood_values,
                                                   current,
                                                   x + offset.x,
                                                   y + offset.y,
                                                   z + offset.z))
            {
                frontier->push_back({x, y, z});
                break;
            }
        }
    }
}

void propagate_skylight(const block_t* neighborhood_blocks,
                        Uint8* neighborhood_values,
                        std::vector<skylight_node_t>* frontier)
{
    std::size_t cursor = 0;
    while (cursor < frontier->size())
    {
        const skylight_node_t node = (*frontier)[cursor++];
        const Uint8 current = neighborhood_values[neighborhood_index(node.x, node.y, node.z)];
        for (const skylight_node_t offset : kPropagationOffsets)
        {
            const int nx = node.x + offset.x;
            const int ny = node.y + offset.y;
            const int nz = node.z + offset.z;
            if (nx < 0 || nx >= kNeighborhoodWidth || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= kNeighborhoodDepth)
            {
                continue;
            }

            const Uint8 propagated = propagated_skylight(current, sample_block(neighborhood_blocks, nx, ny, nz));
            if (propagated == 0)
            {
                continue;
            }

            const std::size_t index = neighborhood_index(nx, ny, nz);
            if (propagated <= neighborhood_values[index])
            {
                continue;
            }

            neighborhood_values[index] = propagated;
            frontier->push_back({nx, ny, nz});
        }
    }
}

} // namespace

auto output_value_count() -> std::size_t
{
    return static_cast<std::size_t>(kOutputWidth) * CHUNK_HEIGHT * kOutputDepth;
}

auto output_index(int x, int y, int z) -> std::size_t
{
    return static_cast<std::size_t>((x * CHUNK_HEIGHT + y) * kOutputDepth + z);
}

void build(const block_t* neighborhood_blocks, Uint8* out_values)
{
    std::fill(out_values, out_values + output_value_count(), static_cast<Uint8>(0));
    std::vector<Uint8> neighborhood_values(neighborhood_value_count(), 0);
    std::vector<skylight_node_t> frontier{};
    frontier.reserve(neighborhood_value_count());

    seed_direct_skylight(neighborhood_blocks, neighborhood_values.data());
    seed_lateral_frontier(neighborhood_blocks, neighborhood_values.data(), &frontier);
    propagate_skylight(neighborhood_blocks, neighborhood_values.data(), &frontier);

    for (int x = 0; x < kOutputWidth; ++x)
    {
        const int sample_x = x + CHUNK_WIDTH - kHalo;
        for (int z = 0; z < kOutputDepth; ++z)
        {
            const int sample_z = z + CHUNK_WIDTH - kHalo;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y)
            {
                out_values[output_index(x, y, z)] = neighborhood_values[neighborhood_index(sample_x, y, sample_z)];
            }
        }
    }
}

} // namespace skylight_cpu
