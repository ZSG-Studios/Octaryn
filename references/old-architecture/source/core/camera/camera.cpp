#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "core/camera/internal.h"
#include "core/check.h"

#define DEGREES(rad) ((rad) * 180.0f / SDL_PI_F)
#define RADIANS(deg) ((deg) * SDL_PI_F / 180.0f)

namespace {

auto camera_normalize_yaw(float yaw) -> float
{
    yaw = SDL_fmodf(yaw + SDL_PI_F, 2.0f * SDL_PI_F);
    if (yaw < 0.0f)
    {
        yaw += 2.0f * SDL_PI_F;
    }
    return yaw - SDL_PI_F;
}

auto camera_zoom_factor(const camera_t* camera) -> float
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

auto camera_perspective_fov(const camera_t* camera) -> float
{
    const float zoom = camera_zoom_factor(camera);
    return 2.0f * SDL_atanf(SDL_tanf(camera->fov * 0.5f) / zoom);
}

} // namespace

void camera_init(camera_t* camera, camera_type_t type)
{
    camera->type = type;
    camera->position[0] = 0.0f;
    camera->position[1] = 0.0f;
    camera->position[2] = 0.0f;
    camera->pitch = 0.0f;
    camera->yaw = 0.0f;
    camera->viewport_size[0] = 1;
    camera->viewport_size[1] = 1;
    camera->fov = RADIANS(90.0f);
    camera->zoom_step = 0;
    camera->near = 0.1f;
    camera->far = 1000.0f;
    camera->ortho = 100.0f;
}

void camera_update(camera_t* camera)
{
    camera->yaw = camera_normalize_yaw(camera->yaw);
    float s = SDL_sinf(camera->yaw);
    float c = SDL_cosf(camera->yaw);
    camera_matrix_translate(camera->view, -camera->position[0], -camera->position[1], -camera->position[2]);
    camera_matrix_rotate(camera->proj, c, 0.0f, s, camera->pitch);
    camera_matrix_multiply(camera->view, camera->proj, camera->view);
    camera_matrix_rotate(camera->proj, 0.0f, 1.0f, 0.0f, -camera->yaw);
    camera_matrix_multiply(camera->view, camera->proj, camera->view);
    const float aspect = static_cast<float>(camera->viewport_size[0]) / static_cast<float>(camera->viewport_size[1]);
    if (camera->type == CAMERA_TYPE_PERSPECTIVE)
    {
        camera_matrix_perspective(camera->proj, aspect, camera_perspective_fov(camera), camera->near, camera->far);
    }
    else
    {
        float ox = camera->ortho * aspect;
        float oy = camera->ortho;
        camera_matrix_ortho(camera->proj, -ox, ox, -oy, oy, camera->near, camera->far);
    }
    camera_matrix_multiply(camera->matrix, camera->proj, camera->view);
    camera_matrix_extract_frustum(camera->planes, camera->matrix);

    float relative_view[4][4];
    SDL_memcpy(relative_view, camera->view, sizeof(relative_view));
    relative_view[3][0] = 0.0f;
    relative_view[3][1] = 0.0f;
    relative_view[3][2] = 0.0f;

    float relative_matrix[4][4];
    camera_matrix_multiply(relative_matrix, camera->proj, relative_view);
    camera_matrix_extract_frustum(camera->relative_planes, relative_matrix);
}

void camera_move(camera_t* camera, float x, float y, float z)
{
    float sy = SDL_sinf(camera->yaw);
    float cy = SDL_cosf(camera->yaw);
    float sp = SDL_sinf(camera->pitch);
    float cp = SDL_cosf(camera->pitch);
    camera->position[0] += cp * (sy * z) + cy * x;
    camera->position[1] += y + z * sp;
    camera->position[2] -= cp * (cy * z) - sy * x;
    camera->position[1] = SDL_clamp(camera->position[1], -camera->far, camera->far);
}

void camera_resize(camera_t* camera, int width, int height)
{
    CHECK(width > 0);
    CHECK(height > 0);
    camera->viewport_size[0] = width;
    camera->viewport_size[1] = height;
}

void camera_rotate(camera_t* camera, float pitch, float yaw)
{
    static const float PITCH = SDL_PI_F / 2.0f - SDL_FLT_EPSILON;
    camera->pitch += RADIANS(pitch);
    camera->yaw = camera_normalize_yaw(camera->yaw + RADIANS(yaw));
    camera->pitch = SDL_clamp(camera->pitch, -PITCH, PITCH);
}

void camera_cycle_zoom(camera_t* camera)
{
    camera->zoom_step = (camera->zoom_step + 1) % 3;
}

void camera_get_vector(const camera_t* camera, float* x, float* y, float* z)
{
    float c = SDL_cosf(camera->pitch);
    *x = SDL_cosf(camera->yaw - RADIANS(90.0f)) * c;
    *y = SDL_sinf(camera->pitch);
    *z = SDL_sinf(camera->yaw - RADIANS(90.0f)) * c;
}

bool camera_get_vis(const camera_t* camera, float x, float y, float z, float sx, float sy, float sz)
{
    x -= camera->position[0];
    y -= camera->position[1];
    z -= camera->position[2];
    float x2 = x + sx;
    float y2 = y + sy;
    float z2 = z + sz;
    for (int i = 0; i < 6; ++i)
    {
        const float *plane = camera->relative_planes[i];
        float a = plane[0] >= 0.0f ? x2 : x;
        float b = plane[1] >= 0.0f ? y2 : y;
        float c = plane[2] >= 0.0f ? z2 : z;
        if (plane[0] * a + plane[1] * b + plane[2] * c + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}
