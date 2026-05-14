#include <SDL3/SDL.h>

#include "internal.h"

namespace
{
constexpr float WALK_SPEED = 5.0f;
constexpr float SPRINT_SPEED = 9.0f;
constexpr float AIR_ACCELERATION = 6.0f;
constexpr float GRAVITY = 24.0f;
constexpr float JUMP_SPEED = 8.5f;
constexpr float FLY_SPEED = 0.01f;
constexpr float FLY_FAST_SPEED = 0.1f;
}

void player_move_walk(player_t* player, float dt, const bool* keys)
{
    const aabb_t aabb = player_get_aabb();
    dt = SDL_min(dt * 0.001f, 0.05f);

    float input_x = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
    float input_z = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
    float length = SDL_sqrtf(input_x * input_x + input_z * input_z);
    if (length > SDL_FLT_EPSILON)
    {
        input_x /= length;
        input_z /= length;
    }

    float speed = keys[SDL_SCANCODE_LCTRL] ? SPRINT_SPEED : WALK_SPEED;
    float sy = SDL_sinf(player->camera.yaw);
    float cy = SDL_cosf(player->camera.yaw);
    float target_x = (cy * input_x + sy * input_z) * speed;
    float target_z = (sy * input_x - cy * input_z) * speed;
    if (player->is_on_ground)
    {
        player->velocity[0] = target_x;
        player->velocity[2] = target_z;
    }
    else
    {
        float blend = SDL_min(1.0f, AIR_ACCELERATION * dt);
        player->velocity[0] += (target_x - player->velocity[0]) * blend;
        player->velocity[2] += (target_z - player->velocity[2]) * blend;
    }

    const bool jump_requested = keys[SDL_SCANCODE_SPACE] && player->is_on_ground;
    if (jump_requested)
    {
        player->velocity[1] = JUMP_SPEED;
        player->is_on_ground = false;
    }
    else if (player->is_on_ground && player->velocity[1] < 0.0f)
    {
        player->velocity[1] = 0.0f;
    }

    if (dt <= SDL_FLT_EPSILON)
    {
        return;
    }

    bool hits[3] = {};
    hits[0] = player_move_axis(&aabb, player->camera.position, 0, player->velocity[0] * dt);
    hits[2] = player_move_axis(&aabb, player->camera.position, 2, player->velocity[2] * dt);

    if (hits[0])
    {
        player->velocity[0] = 0.0f;
    }
    if (hits[2])
    {
        player->velocity[2] = 0.0f;
    }

    if (!jump_requested && player->is_on_ground && player_has_ground_contact(&aabb, player->camera.position))
    {
        player->velocity[1] = 0.0f;
        player_update_query(player);
        return;
    }

    player->is_on_ground = false;
    player->velocity[1] -= GRAVITY * dt;
    hits[1] = player_move_axis(&aabb, player->camera.position, 1, player->velocity[1] * dt);
    if (hits[1])
    {
        if (player->velocity[1] < 0.0f)
        {
            player->is_on_ground = true;
        }
        player->velocity[1] = 0.0f;
    }

    player_update_query(player);
}

void player_move_fly(player_t* player, float dt, const bool* keys)
{
    float speed = keys[SDL_SCANCODE_LCTRL] ? FLY_FAST_SPEED : FLY_SPEED;
    float dx = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
    float dy = (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE]) -
               (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LSHIFT]);
    float dz = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
    camera_move(&player->camera, dx * speed * dt, dy * speed * dt, dz * speed * dt);
    player_update_query(player);
}
