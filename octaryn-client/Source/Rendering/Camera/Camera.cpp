#include "Camera.h"

#include "CameraMatrix.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float FloatEpsilon = 1.192092896e-07f;
constexpr int ZoomStepCount = 3;

float radians_from_degrees(float degrees)
{
    return degrees * Pi / 180.0f;
}

float normalize_yaw(float yaw)
{
    yaw = std::fmod(yaw + Pi, 2.0f * Pi);
    if (yaw < 0.0f)
    {
        yaw += 2.0f * Pi;
    }

    return yaw - Pi;
}

float zoom_factor(const camera* camera)
{
    if (camera->zoom_step == 1)
    {
        return 2.0f;
    }

    if (camera->zoom_step == 2)
    {
        return 4.0f;
    }

    return 1.0f;
}

float perspective_field_of_view(const camera* camera)
{
    return 2.0f * std::atan(std::tan(camera->vertical_field_of_view_radians * 0.5f) / zoom_factor(camera));
}

int positive_dimension_or_default(int value)
{
    return value > 0 ? value : 1;
}

} // namespace

void camera_init(
    camera* camera,
    camera_projection projection_mode)
{
    std::memset(camera, 0, sizeof(*camera));
    camera->projection_mode = projection_mode;
    camera->viewport_width = 1;
    camera->viewport_height = 1;
    camera->vertical_field_of_view_radians = radians_from_degrees(90.0f);
    camera->near_plane = 0.1f;
    camera->far_plane = 1000.0f;
    camera->orthographic_size = 100.0f;
}

void camera_update(camera* camera)
{
    camera->yaw_radians = normalize_yaw(camera->yaw_radians);

    const float yaw_sine = std::sin(camera->yaw_radians);
    const float yaw_cosine = std::cos(camera->yaw_radians);
    float rotation[4][4];

    camera_matrix_translate(
        camera->view,
        -camera->position[0],
        -camera->position[1],
        -camera->position[2]);
    camera_matrix_rotate(rotation, yaw_cosine, 0.0f, yaw_sine, camera->pitch_radians);
    camera_matrix_multiply(camera->view, rotation, camera->view);
    camera_matrix_rotate(rotation, 0.0f, 1.0f, 0.0f, -camera->yaw_radians);
    camera_matrix_multiply(camera->view, rotation, camera->view);

    const int viewport_width = positive_dimension_or_default(camera->viewport_width);
    const int viewport_height = positive_dimension_or_default(camera->viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    if (camera->projection_mode == CAMERA_PROJECTION_PERSPECTIVE)
    {
        camera_matrix_perspective(
            camera->projection,
            aspect,
            perspective_field_of_view(camera),
            camera->near_plane,
            camera->far_plane);
    }
    else
    {
        const float orthographic_x = camera->orthographic_size * aspect;
        const float orthographic_y = camera->orthographic_size;
        camera_matrix_orthographic(
            camera->projection,
            -orthographic_x,
            orthographic_x,
            -orthographic_y,
            orthographic_y,
            camera->near_plane,
            camera->far_plane);
    }

    camera_matrix_multiply(camera->view_projection, camera->projection, camera->view);
    camera_matrix_extract_frustum(camera->frustum_planes, camera->view_projection);

    float relative_view[4][4];
    std::memcpy(relative_view, camera->view, sizeof(relative_view));
    relative_view[3][0] = 0.0f;
    relative_view[3][1] = 0.0f;
    relative_view[3][2] = 0.0f;

    float relative_view_projection[4][4];
    camera_matrix_multiply(relative_view_projection, camera->projection, relative_view);
    camera_matrix_extract_frustum(camera->relative_frustum_planes, relative_view_projection);
}

void camera_move(camera* camera, float x, float y, float z)
{
    const float yaw_sine = std::sin(camera->yaw_radians);
    const float yaw_cosine = std::cos(camera->yaw_radians);
    const float pitch_sine = std::sin(camera->pitch_radians);
    const float pitch_cosine = std::cos(camera->pitch_radians);

    camera->position[0] += pitch_cosine * (yaw_sine * z) + yaw_cosine * x;
    camera->position[1] += y + z * pitch_sine;
    camera->position[2] -= pitch_cosine * (yaw_cosine * z) - yaw_sine * x;
    camera->position[1] = std::clamp(camera->position[1], -camera->far_plane, camera->far_plane);
}

void camera_resize(camera* camera, int width, int height)
{
    camera->viewport_width = positive_dimension_or_default(width);
    camera->viewport_height = positive_dimension_or_default(height);
}

void camera_rotate_degrees(camera* camera, float pitch, float yaw)
{
    constexpr float PitchLimit = Pi / 2.0f - FloatEpsilon;
    camera->pitch_radians = std::clamp(
        camera->pitch_radians + radians_from_degrees(pitch),
        -PitchLimit,
        PitchLimit);
    camera->yaw_radians = normalize_yaw(camera->yaw_radians + radians_from_degrees(yaw));
}

void camera_cycle_zoom(camera* camera)
{
    camera->zoom_step = (camera->zoom_step + 1) % ZoomStepCount;
}

void camera_forward_vector(
    const camera* camera,
    float* x,
    float* y,
    float* z)
{
    const float pitch_cosine = std::cos(camera->pitch_radians);
    *x = std::cos(camera->yaw_radians - radians_from_degrees(90.0f)) * pitch_cosine;
    *y = std::sin(camera->pitch_radians);
    *z = std::sin(camera->yaw_radians - radians_from_degrees(90.0f)) * pitch_cosine;
}

int camera_is_box_visible(
    const camera* camera,
    float x,
    float y,
    float z,
    float width,
    float height,
    float depth)
{
    x -= camera->position[0];
    y -= camera->position[1];
    z -= camera->position[2];

    const float x_max = x + width;
    const float y_max = y + height;
    const float z_max = z + depth;

    for (int i = 0; i < 6; ++i)
    {
        const float* plane = camera->relative_frustum_planes[i];
        const float box_x = plane[0] >= 0.0f ? x_max : x;
        const float box_y = plane[1] >= 0.0f ? y_max : y;
        const float box_z = plane[2] >= 0.0f ? z_max : z;
        if (plane[0] * box_x + plane[1] * box_y + plane[2] * box_z + plane[3] < 0.0f)
        {
            return 0;
        }
    }

    return 1;
}
