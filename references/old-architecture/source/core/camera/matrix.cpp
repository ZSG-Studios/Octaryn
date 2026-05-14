#include <SDL3/SDL.h>

#include "core/camera/internal.h"

void camera_matrix_multiply(float matrix[4][4], float a[4][4], float b[4][4])
{
    float c[4][4];
    for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
        c[i][j] = 0.0f;
        c[i][j] += a[0][j] * b[i][0];
        c[i][j] += a[1][j] * b[i][1];
        c[i][j] += a[2][j] * b[i][2];
        c[i][j] += a[3][j] * b[i][3];
    }
    for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
        matrix[i][j] = c[i][j];
    }
}

void camera_matrix_perspective(float matrix[4][4], float aspect, float fov, float near, float far)
{
    const float tan_half_fov = SDL_tanf(fov / 2.0f);
    matrix[0][0] = 1.0f / (aspect * tan_half_fov);
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 1.0f / tan_half_fov;
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = far / (near - far);
    matrix[2][3] = -1.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = (far * near) / (near - far);
    matrix[3][3] = 0.0f;
}

void camera_matrix_ortho(float matrix[4][4], float left, float right, float bottom, float top, float near, float far)
{
    matrix[0][0] = 2.0f / (right - left);
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 2.0f / (top - bottom);
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = 1.0f / (near - far);
    matrix[2][3] = 0.0f;
    matrix[3][0] = -(right + left) / (right - left);
    matrix[3][1] = -(top + bottom) / (top - bottom);
    matrix[3][2] = near / (near - far);
    matrix[3][3] = 1.0f;
}

void camera_matrix_translate(float matrix[4][4], float x, float y, float z)
{
    matrix[0][0] = 1.0f;
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 1.0f;
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = 1.0f;
    matrix[2][3] = 0.0f;
    matrix[3][0] = x;
    matrix[3][1] = y;
    matrix[3][2] = z;
    matrix[3][3] = 1.0f;
}

void camera_matrix_extract_frustum(float planes[6][4], float matrix[4][4])
{
    planes[0][0] = matrix[0][3] + matrix[0][0];
    planes[0][1] = matrix[1][3] + matrix[1][0];
    planes[0][2] = matrix[2][3] + matrix[2][0];
    planes[0][3] = matrix[3][3] + matrix[3][0];
    planes[1][0] = matrix[0][3] - matrix[0][0];
    planes[1][1] = matrix[1][3] - matrix[1][0];
    planes[1][2] = matrix[2][3] - matrix[2][0];
    planes[1][3] = matrix[3][3] - matrix[3][0];
    planes[2][0] = matrix[0][3] + matrix[0][1];
    planes[2][1] = matrix[1][3] + matrix[1][1];
    planes[2][2] = matrix[2][3] + matrix[2][1];
    planes[2][3] = matrix[3][3] + matrix[3][1];
    planes[3][0] = matrix[0][3] - matrix[0][1];
    planes[3][1] = matrix[1][3] - matrix[1][1];
    planes[3][2] = matrix[2][3] - matrix[2][1];
    planes[3][3] = matrix[3][3] - matrix[3][1];
    planes[4][0] = matrix[0][3] + matrix[0][2];
    planes[4][1] = matrix[1][3] + matrix[1][2];
    planes[4][2] = matrix[2][3] + matrix[2][2];
    planes[4][3] = matrix[3][3] + matrix[3][2];
    planes[5][0] = matrix[0][3] - matrix[0][2];
    planes[5][1] = matrix[1][3] - matrix[1][2];
    planes[5][2] = matrix[2][3] - matrix[2][2];
    planes[5][3] = matrix[3][3] - matrix[3][2];
    for (int i = 0; i < 6; ++i)
    {
        float length = 0.0f;
        length += planes[i][0] * planes[i][0];
        length += planes[i][1] * planes[i][1];
        length += planes[i][2] * planes[i][2];
        length = SDL_sqrtf(length);
        if (length < SDL_FLT_EPSILON)
        {
            continue;
        }
        planes[i][0] /= length;
        planes[i][1] /= length;
        planes[i][2] /= length;
        planes[i][3] /= length;
    }
}

void camera_matrix_rotate(float matrix[4][4], float x, float y, float z, float angle)
{
    float s = SDL_sinf(angle);
    float c = SDL_cosf(angle);
    float i = 1.0f - c;
    matrix[0][0] = i * x * x + c;
    matrix[0][1] = i * x * y - z * s;
    matrix[0][2] = i * z * x + y * s;
    matrix[0][3] = 0.0f;
    matrix[1][0] = i * x * y + z * s;
    matrix[1][1] = i * y * y + c;
    matrix[1][2] = i * y * z - x * s;
    matrix[1][3] = 0.0f;
    matrix[2][0] = i * z * x - y * s;
    matrix[2][1] = i * y * z + x * s;
    matrix[2][2] = i * z * z + c;
    matrix[2][3] = 0.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
}
