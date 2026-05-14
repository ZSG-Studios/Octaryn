#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "world/runtime/world.h"

world_query_t world_raycast(const camera_t* camera, float length)
{
    world_query_t query = {};
    float direction[3] = {};
    float distances[3] = {};
    int steps[3] = {};
    float deltas[3] = {};
    camera_get_vector(camera, &direction[0], &direction[1], &direction[2]);
    for (int i = 0; i < 3; i++)
    {
        query.current[i] = static_cast<int>(SDL_floorf(camera->position[i]));
        query.previous[i] = query.current[i];
        if (SDL_fabsf(direction[i]) > SDL_FLT_EPSILON)
        {
            deltas[i] = SDL_fabsf(1.0f / direction[i]);
        }
        else
        {
            deltas[i] = 1e6f;
        }
        if (direction[i] < 0.0f)
        {
            steps[i] = -1;
            distances[i] = (camera->position[i] - static_cast<float>(query.current[i])) * deltas[i];
        }
        else
        {
            steps[i] = 1;
            distances[i] = (static_cast<float>(query.current[i]) + 1.0f - camera->position[i]) * deltas[i];
        }
    }
    float traveled = 0.0f;
    while (traveled <= length)
    {
        query.block = world_get_block(query.current);
        if (block_is_targetable(query.block))
        {
            return query;
        }
        for (int i = 0; i < 3; i++)
        {
            query.previous[i] = query.current[i];
        }
        if (distances[0] < distances[1])
        {
            if (distances[0] < distances[2])
            {
                traveled = distances[0];
                distances[0] += deltas[0];
                query.current[0] += steps[0];
            }
            else
            {
                traveled = distances[2];
                distances[2] += deltas[2];
                query.current[2] += steps[2];
            }
        }
        else
        {
            if (distances[1] < distances[2])
            {
                traveled = distances[1];
                distances[1] += deltas[1];
                query.current[1] += steps[1];
            }
            else
            {
                traveled = distances[2];
                distances[2] += deltas[2];
                query.current[2] += steps[2];
            }
        }
    }
    query.block = BLOCK_EMPTY;
    return query;
}
