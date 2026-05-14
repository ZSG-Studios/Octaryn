#include <SDL3/SDL.h>

#include "internal.h"

void player_move(player_t* player, float dt)
{
    const bool* keys = SDL_GetKeyboardState(NULL);
    if (player->controller == PLAYER_CONTROLLER_WALK)
    {
        player_move_walk(player, dt, keys);
        return;
    }

    player_move_fly(player, dt, keys);
}
