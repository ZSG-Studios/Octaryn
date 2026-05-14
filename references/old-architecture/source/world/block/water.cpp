#include "world/block/block.h"

bool block_is_water(block_t block)
{
    return block >= BLOCK_WATER && block <= BLOCK_WATER_7;
}

bool block_is_water_source(block_t block)
{
    return block == BLOCK_WATER;
}

int block_get_water_level(block_t block)
{
    if (block == BLOCK_WATER)
    {
        return 0;
    }
    if (block >= BLOCK_WATER_1 && block <= BLOCK_WATER_7)
    {
        return block - BLOCK_WATER_1 + 1;
    }
    return -1;
}

block_t block_make_water(int level)
{
    if (level <= 0)
    {
        return BLOCK_WATER;
    }
    if (level >= 7)
    {
        return BLOCK_WATER_7;
    }
    return static_cast<block_t>(BLOCK_WATER_1 + level - 1);
}

bool block_is_lava(block_t block)
{
    return block >= BLOCK_LAVA && block <= BLOCK_LAVA_7;
}

bool block_is_lava_source(block_t block)
{
    return block == BLOCK_LAVA;
}

int block_get_lava_level(block_t block)
{
    if (block == BLOCK_LAVA)
    {
        return 0;
    }
    if (block >= BLOCK_LAVA_1 && block <= BLOCK_LAVA_7)
    {
        return block - BLOCK_LAVA_1 + 1;
    }
    return -1;
}

block_t block_make_lava(int level)
{
    if (level <= 0)
    {
        return BLOCK_LAVA;
    }
    if (level >= 7)
    {
        return BLOCK_LAVA_7;
    }
    return static_cast<block_t>(BLOCK_LAVA_1 + level - 1);
}

bool block_is_fluid(block_t block)
{
    return block_is_water(block) || block_is_lava(block);
}

bool block_is_fluid_source(block_t block)
{
    return block_is_water_source(block) || block_is_lava_source(block);
}

fluid_kind_t block_get_fluid_kind(block_t block)
{
    if (block_is_water(block))
    {
        return FLUID_WATER;
    }
    if (block_is_lava(block))
    {
        return FLUID_LAVA;
    }
    return FLUID_NONE;
}

int block_get_fluid_level(block_t block)
{
    if (block_is_water(block))
    {
        return block_get_water_level(block);
    }
    if (block_is_lava(block))
    {
        return block_get_lava_level(block);
    }
    return -1;
}

block_t block_make_fluid(fluid_kind_t kind, int level)
{
    if (kind == FLUID_WATER)
    {
        return block_make_water(level);
    }
    if (kind == FLUID_LAVA)
    {
        return block_make_lava(level);
    }
    return BLOCK_EMPTY;
}
