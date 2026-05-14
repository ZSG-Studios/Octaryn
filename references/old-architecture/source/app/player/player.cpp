#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "player.h"
#include "world/runtime/world.h"

static const float SENSITIVITY = 0.1f;
static const float REACH = 10.0f;
void player_update_query(player_t* player)
{
    player->query = world_raycast(&player->camera, REACH);
}

void player_toggle_controller(player_t* player)
{
    const int next_controller = (static_cast<int>(player->controller) + 1) % PLAYER_CONTROLLER_COUNT;
    player->controller = static_cast<player_controller_t>(next_controller);
}

void player_start_flight(player_t* player)
{
    player->controller = PLAYER_CONTROLLER_FLY;
}

void player_rotate(player_t* player, float pitch, float yaw)
{
    camera_rotate(&player->camera, pitch * -SENSITIVITY, yaw * SENSITIVITY);
    player_update_query(player);
}
