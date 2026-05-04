#include "RenderDistance.h"

namespace {

constexpr int RenderDistanceOptions[] = {4, 8, 12, 16, 20, 24, 32};
constexpr int RenderDistanceOptionCount = 7;

} // namespace

int render_distance_option_count(void)
{
    return RenderDistanceOptionCount;
}

const int* render_distance_options(void)
{
    return RenderDistanceOptions;
}

int render_distance_sanitize(int distance)
{
    for (int index = RenderDistanceOptionCount - 1; index >= 0; --index)
    {
        if (distance >= RenderDistanceOptions[index])
        {
            return RenderDistanceOptions[index];
        }
    }

    return RenderDistanceOptions[0];
}

int render_distance_next_step(int current_distance, int target_distance)
{
    const int sanitized_current = render_distance_sanitize(current_distance);
    const int sanitized_target = render_distance_sanitize(target_distance);
    if (sanitized_current >= sanitized_target)
    {
        return sanitized_target;
    }

    for (int index = 0; index < RenderDistanceOptionCount; ++index)
    {
        const int option = RenderDistanceOptions[index];
        if (option > sanitized_current)
        {
            return option < sanitized_target ? option : sanitized_target;
        }
    }

    return sanitized_target;
}
