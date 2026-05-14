#pragma once

#include "render/scene/passes.h"

inline constexpr Uint32 kMaterialFlagPbr = 1u << 0u;
inline constexpr Uint32 kMaterialFlagPom = 1u << 1u;

inline auto main_render_get_query_hit_face(const player_t* player) -> Uint32
{
    if (player->query.block == BLOCK_EMPTY)
    {
        return DIRECTION_COUNT;
    }

    const int dx = player->query.current[0] - player->query.previous[0];
    const int dy = player->query.current[1] - player->query.previous[1];
    const int dz = player->query.current[2] - player->query.previous[2];
    if (dx > 0) return DIRECTION_WEST;
    if (dx < 0) return DIRECTION_EAST;
    if (dy > 0) return DIRECTION_DOWN;
    if (dy < 0) return DIRECTION_UP;
    if (dz > 0) return DIRECTION_SOUTH;
    if (dz < 0) return DIRECTION_NORTH;
    return DIRECTION_COUNT;
}
