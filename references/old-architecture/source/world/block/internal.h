#pragma once

#include <array>

#include "world/block/block.h"

struct block_definition_t
{
    bool is_opaque = false;
    bool is_sprite = false;
    bool is_solid = false;
    bool has_occlusion = false;
    std::array<int, DIRECTION_COUNT> indices{};
};

auto block_definition(block_t block) -> const block_definition_t&;
