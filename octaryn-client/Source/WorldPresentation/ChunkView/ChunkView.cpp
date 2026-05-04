#include "ChunkView.h"

#include "octaryn_client_render_distance.h"

#include <algorithm>
#include <cmath>

namespace {

int clamp_view_width(int width)
{
    return std::clamp(
        width,
        CHUNK_VIEW_MIN_WIDTH,
        CHUNK_VIEW_MAX_WIDTH);
}

} // namespace

int chunk_origin_for_position(float position, int view_width)
{
    const int sanitized_width = clamp_view_width(view_width);
    return static_cast<int>(
               std::floor(position / static_cast<float>(CHUNK_VIEW_CHUNK_WIDTH))) -
           sanitized_width / 2;
}

chunk_view chunk_view_for_camera(
    float camera_x,
    float camera_z,
    int render_distance)
{
    const int sanitized_distance =
        octaryn_client_render_distance_sanitize(render_distance);
    const int view_width = clamp_view_width(sanitized_distance * 2 + 1);
    return {
        chunk_origin_for_position(camera_x, view_width),
        chunk_origin_for_position(camera_z, view_width),
        view_width,
    };
}

int chunk_view_equal(
    const chunk_view* left,
    const chunk_view* right)
{
    if (left == nullptr || right == nullptr)
    {
        return 0;
    }

    return left->origin_x == right->origin_x &&
           left->origin_z == right->origin_z &&
           left->width == right->width;
}
