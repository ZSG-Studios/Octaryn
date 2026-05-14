#pragma once

#include "../internal.h"

bool player_move_axis(const aabb_t* aabb, float position[3], int axis, float delta);
bool player_has_ground_contact(const aabb_t* aabb, const float position[3]);
void player_move_walk(player_t* player, float dt, const bool* keys);
void player_move_fly(player_t* player, float dt, const bool* keys);
