#pragma once

#include "core/camera/camera.h"

void camera_matrix_multiply(float matrix[4][4], float a[4][4], float b[4][4]);
void camera_matrix_perspective(float matrix[4][4], float aspect, float fov, float near, float far);
void camera_matrix_ortho(float matrix[4][4], float left, float right, float bottom, float top, float near, float far);
void camera_matrix_translate(float matrix[4][4], float x, float y, float z);
void camera_matrix_rotate(float matrix[4][4], float x, float y, float z, float angle);
void camera_matrix_extract_frustum(float planes[6][4], float matrix[4][4]);
