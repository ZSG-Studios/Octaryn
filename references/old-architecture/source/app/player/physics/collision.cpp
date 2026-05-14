#include <SDL3/SDL.h>

#include "internal.h"

namespace
{
constexpr float COLLISION_STEP = 0.1f;
constexpr float COLLISION_RADIUS = 0.3f;
constexpr float COLLISION_HEIGHT = 1.8f;
constexpr float EYE_OFFSET = 1.62f;
constexpr float GROUND_CONTACT_PROBE = 0.025f;

bool is_solid(const float position[3])
{
    int index[3] = {
        static_cast<int>(SDL_floorf(position[0])),
        static_cast<int>(SDL_floorf(position[1])),
        static_cast<int>(SDL_floorf(position[2])),
    };
    return block_is_solid(world_get_block(index));
}

bool is_colliding(const aabb_t* aabb, const float position[3])
{
    int min[3];
    int max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = static_cast<int>(SDL_floorf(position[i] + aabb->min[i] + PLAYER_PHYSICS_EPSILON));
        max[i] = static_cast<int>(SDL_floorf(position[i] + aabb->max[i] - PLAYER_PHYSICS_EPSILON));
    }

    for (int bx = min[0]; bx <= max[0]; bx++)
    for (int by = min[1]; by <= max[1]; by++)
    for (int bz = min[2]; bz <= max[2]; bz++)
    {
        float location[3] = {static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz)};
        if (is_solid(location))
        {
            return true;
        }
    }

    return false;
}

void bisect(const aabb_t* aabb, float position[3], int axis, float step)
{
    float start[3] = {position[0], position[1], position[2]};
    float lower = 0.0f;
    float upper = 1.0f;
    for (int i = 0; i < 8; i++)
    {
        float t = (lower + upper) * 0.5f;
        float location[3] = {start[0], start[1], start[2]};
        location[axis] += step * t;
        if (is_colliding(aabb, location))
        {
            upper = t;
        }
        else
        {
            lower = t;
        }
    }

    position[axis] = start[axis] + step * lower;
}
}

aabb_t player_get_aabb(void)
{
    return {{-COLLISION_RADIUS, -EYE_OFFSET, -COLLISION_RADIUS},
            {COLLISION_RADIUS, COLLISION_HEIGHT - EYE_OFFSET, COLLISION_RADIUS}};
}

bool player_move_axis(const aabb_t* aabb, float position[3], int axis, float delta)
{
    if (SDL_fabsf(delta) <= SDL_FLT_EPSILON)
    {
        return false;
    }

    int steps = static_cast<int>(SDL_ceilf(SDL_fabsf(delta) / COLLISION_STEP));
    steps = SDL_max(steps, 1);
    float step = delta / static_cast<float>(steps);
    for (int i = 0; i < steps; i++)
    {
        float location[3] = {position[0], position[1], position[2]};
        location[axis] += step;
        if (is_colliding(aabb, location))
        {
            bisect(aabb, position, axis, step);
            return true;
        }
        SDL_memcpy(position, location, 12);
    }

    return false;
}

bool player_has_ground_contact(const aabb_t* aabb, const float position[3])
{
    float probe[3] = {position[0], position[1] - GROUND_CONTACT_PROBE, position[2]};
    return is_colliding(aabb, probe);
}
