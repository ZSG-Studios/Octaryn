#pragma once

#include <SDL3/SDL.h>

#include "world/direction.h"

typedef Uint8 block_t;
enum // block_t
{
    BLOCK_EMPTY,

    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_CLOUD,
    BLOCK_BUSH,
    BLOCK_BLUEBELL,
    BLOCK_GARDENIA,
    BLOCK_ROSE,
    BLOCK_LAVENDER,
    BLOCK_WATER,
    BLOCK_WATER_1,
    BLOCK_WATER_2,
    BLOCK_WATER_3,
    BLOCK_WATER_4,
    BLOCK_WATER_5,
    BLOCK_WATER_6,
    BLOCK_WATER_7,
    BLOCK_RED_TORCH,
    BLOCK_GREEN_TORCH,
    BLOCK_BLUE_TORCH,
    BLOCK_YELLOW_TORCH,
    BLOCK_CYAN_TORCH,
    BLOCK_MAGENTA_TORCH,
    BLOCK_WHITE_TORCH,
    BLOCK_PLANKS,
    BLOCK_GLASS,
    BLOCK_LAVA,
    BLOCK_LAVA_1,
    BLOCK_LAVA_2,
    BLOCK_LAVA_3,
    BLOCK_LAVA_4,
    BLOCK_LAVA_5,
    BLOCK_LAVA_6,
    BLOCK_LAVA_7,

    BLOCK_COUNT,
};

enum fluid_kind_t
{
    FLUID_NONE,
    FLUID_WATER,
    FLUID_LAVA,
};

bool block_is_opaque(block_t block);
bool block_is_sprite(block_t block);
bool block_is_solid(block_t block);
bool block_is_placeable(block_t block);
bool block_is_targetable(block_t block);
bool block_has_occlusion(block_t block);
bool block_requires_grass(block_t block);
bool block_requires_solid_base(block_t block);
bool block_is_water(block_t block);
bool block_is_water_source(block_t block);
int block_get_water_level(block_t block);
block_t block_make_water(int level);
bool block_is_lava(block_t block);
bool block_is_lava_source(block_t block);
int block_get_lava_level(block_t block);
block_t block_make_lava(int level);
bool block_is_fluid(block_t block);
bool block_is_fluid_source(block_t block);
fluid_kind_t block_get_fluid_kind(block_t block);
int block_get_fluid_level(block_t block);
block_t block_make_fluid(fluid_kind_t kind, int level);
int block_get_index(block_t block, direction_t direction);
int block_get_skylight_opacity(block_t block);
