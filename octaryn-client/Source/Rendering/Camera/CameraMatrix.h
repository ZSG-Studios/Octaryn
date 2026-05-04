#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void camera_matrix_multiply(float matrix[4][4], const float a[4][4], const float b[4][4]);
void camera_matrix_perspective(
    float matrix[4][4],
    float aspect,
    float field_of_view,
    float near_plane,
    float far_plane);
void camera_matrix_orthographic(
    float matrix[4][4],
    float left,
    float right,
    float bottom,
    float top,
    float near_plane,
    float far_plane);
void camera_matrix_translate(float matrix[4][4], float x, float y, float z);
void camera_matrix_rotate(float matrix[4][4], float x, float y, float z, float angle);
void camera_matrix_extract_frustum(float planes[6][4], const float matrix[4][4]);

#ifdef __cplusplus
}
#endif
