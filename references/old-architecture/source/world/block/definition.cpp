#include "world/block/internal.h"

#include <array>

#include "core/check.h"
#include "render/atlas/config.h"

namespace {

constexpr auto block_indices(int north, int south, int east, int west, int up, int down) -> std::array<int, DIRECTION_COUNT>
{
    return {north, south, east, west, up, down};
}

constexpr auto uniform_indices(int index) -> std::array<int, DIRECTION_COUNT>
{
    return {index, index, index, index, index, index};
}

constexpr auto make_block(
    bool is_opaque,
    bool is_sprite,
    bool is_solid,
    bool has_occlusion,
    std::array<int, DIRECTION_COUNT> indices) -> block_definition_t
{
    return {
        .is_opaque = is_opaque,
        .is_sprite = is_sprite,
        .is_solid = is_solid,
        .has_occlusion = has_occlusion,
        .indices = indices,
    };
}

constexpr auto make_block_table() -> std::array<block_definition_t, BLOCK_COUNT>
{
    std::array<block_definition_t, BLOCK_COUNT> blocks{};
    blocks[BLOCK_GRASS] = make_block(true, false, true, true, block_indices(2, 2, 2, 2, 1, 3));
    blocks[BLOCK_DIRT] = make_block(true, false, true, true, uniform_indices(3));
    blocks[BLOCK_SAND] = make_block(true, false, true, true, uniform_indices(5));
    blocks[BLOCK_SNOW] = make_block(true, false, true, true, uniform_indices(6));
    blocks[BLOCK_STONE] = make_block(true, false, true, true, uniform_indices(4));
    blocks[BLOCK_LOG] = make_block(true, false, true, true, block_indices(8, 8, 8, 8, 7, 7));
    blocks[BLOCK_LEAVES] = make_block(true, false, true, false, uniform_indices(10));
    blocks[BLOCK_CLOUD] = make_block(false, false, false, false, uniform_indices(9));
    blocks[BLOCK_BUSH] = make_block(true, true, false, false, uniform_indices(15));
    blocks[BLOCK_BLUEBELL] = make_block(true, true, false, false, uniform_indices(13));
    blocks[BLOCK_GARDENIA] = make_block(true, true, false, false, uniform_indices(12));
    blocks[BLOCK_ROSE] = make_block(true, true, false, false, uniform_indices(11));
    blocks[BLOCK_LAVENDER] = make_block(true, true, false, false, uniform_indices(14));
    blocks[BLOCK_WATER] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_1] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_2] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_3] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_4] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_5] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_6] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_WATER_7] = make_block(false, false, false, false, uniform_indices(16));
    blocks[BLOCK_RED_TORCH] = make_block(true, true, false, false, uniform_indices(17));
    blocks[BLOCK_GREEN_TORCH] = make_block(true, true, false, false, uniform_indices(18));
    blocks[BLOCK_BLUE_TORCH] = make_block(true, true, false, false, uniform_indices(19));
    blocks[BLOCK_YELLOW_TORCH] = make_block(true, true, false, false, uniform_indices(20));
    blocks[BLOCK_CYAN_TORCH] = make_block(true, true, false, false, uniform_indices(21));
    blocks[BLOCK_MAGENTA_TORCH] = make_block(true, true, false, false, uniform_indices(22));
    blocks[BLOCK_WHITE_TORCH] = make_block(true, true, false, false, uniform_indices(23));
    blocks[BLOCK_PLANKS] = make_block(true, false, true, true, uniform_indices(24));
    blocks[BLOCK_GLASS] = make_block(false, false, true, false, uniform_indices(25));
    blocks[BLOCK_LAVA] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_1] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_2] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_3] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_4] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_5] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_6] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    blocks[BLOCK_LAVA_7] = make_block(false, false, false, false, uniform_indices(MAIN_RENDER_ATLAS_LAVA_STILL_LAYER));
    return blocks;
}

constexpr auto kBlocks = make_block_table();

constexpr bool block_atlas_indices_are_valid(const std::array<block_definition_t, BLOCK_COUNT>& blocks)
{
    for (const block_definition_t& block : blocks)
    {
        for (int index : block.indices)
        {
            if (index < 0 || index >= MAIN_RENDER_ATLAS_LAYER_COUNT)
            {
                return false;
            }
        }
    }
    return true;
}

static_assert(MAIN_RENDER_ATLAS_LAYER_COUNT <= 64);
static_assert(block_atlas_indices_are_valid(kBlocks));

} // namespace

auto block_definition(block_t block) -> const block_definition_t&
{
    CHECK(block < BLOCK_COUNT);
    return kBlocks[block];
}
