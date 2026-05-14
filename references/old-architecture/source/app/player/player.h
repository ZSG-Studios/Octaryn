#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "core/camera/camera.h"
#include "world/runtime/world.h"

typedef enum player_controller
{
    PLAYER_CONTROLLER_WALK,
    PLAYER_CONTROLLER_FLY,
    PLAYER_CONTROLLER_COUNT,
}
player_controller_t;

typedef struct player
{
    camera_t camera;
    player_controller_t controller;
    float velocity[3];
    bool is_on_ground;
    world_query_t query;
    block_t block;
}
player_t;

bool player_save_or_load(player_t* player, int id, bool save);
void player_save_if_due(player_t* player, int id);
void player_reset_spawn(player_t* player);
void player_update_query(player_t* player);
void player_start_flight(player_t* player);
void player_toggle_controller(player_t* player);
void player_rotate(player_t* player, float pitch, float yaw);
void player_move(player_t* player, float dt);
void player_place_block(player_t* player);
void player_select_block(player_t* player);
void player_break_block(player_t* player);
void player_change_block(player_t* player, int dy);
