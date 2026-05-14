#pragma once

#include "player.h"

typedef struct aabb
{
    float min[3];
    float max[3];
}
aabb_t;

inline constexpr float PLAYER_PHYSICS_EPSILON = 0.001f;

void player_apply_default_spawn(player_t* player);
aabb_t player_get_aabb(void);
