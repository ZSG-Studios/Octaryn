#pragma once

inline constexpr int OCTARYN_RENDER_DISTANCE_OPTIONS[] = {4, 8, 12, 16, 20, 24, 32};
inline constexpr int OCTARYN_RENDER_DISTANCE_OPTION_COUNT = 7;

inline int octaryn_sanitize_render_distance(int distance)
{
    for (int index = OCTARYN_RENDER_DISTANCE_OPTION_COUNT - 1; index >= 0; --index)
    {
        if (distance >= OCTARYN_RENDER_DISTANCE_OPTIONS[index])
        {
            return OCTARYN_RENDER_DISTANCE_OPTIONS[index];
        }
    }
    return OCTARYN_RENDER_DISTANCE_OPTIONS[0];
}

inline int octaryn_next_render_distance_step(int current_distance, int target_distance)
{
    const int sanitized_current = octaryn_sanitize_render_distance(current_distance);
    const int sanitized_target = octaryn_sanitize_render_distance(target_distance);
    if (sanitized_current >= sanitized_target)
    {
        return sanitized_target;
    }

    for (int option : OCTARYN_RENDER_DISTANCE_OPTIONS)
    {
        if (option > sanitized_current)
        {
            return option < sanitized_target ? option : sanitized_target;
        }
    }
    return sanitized_target;
}
