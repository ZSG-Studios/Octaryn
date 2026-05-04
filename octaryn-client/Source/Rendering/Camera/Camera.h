#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum camera_projection
{
    CAMERA_PROJECTION_ORTHOGRAPHIC = 0,
    CAMERA_PROJECTION_PERSPECTIVE = 1,
}
camera_projection;

typedef struct camera
{
    camera_projection projection_mode;
    float view[4][4];
    float projection[4][4];
    float view_projection[4][4];
    float frustum_planes[6][4];
    float relative_frustum_planes[6][4];
    float position[3];
    float pitch_radians;
    float yaw_radians;
    int viewport_width;
    int viewport_height;
    float vertical_field_of_view_radians;
    int zoom_step;
    float near_plane;
    float far_plane;
    float orthographic_size;
}
camera;

void camera_init(
    camera* camera,
    camera_projection projection_mode);
void camera_update(camera* camera);
void camera_move(camera* camera, float x, float y, float z);
void camera_resize(camera* camera, int width, int height);
void camera_rotate_degrees(camera* camera, float pitch, float yaw);
void camera_cycle_zoom(camera* camera);
void camera_forward_vector(
    const camera* camera,
    float* x,
    float* y,
    float* z);
int camera_is_box_visible(
    const camera* camera,
    float x,
    float y,
    float z,
    float width,
    float height,
    float depth);

#ifdef __cplusplus
}
#endif
