#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OCTARYN_CLIENT_CHUNK_WIDTH 32
#define OCTARYN_CLIENT_CHUNK_VIEW_MIN_WIDTH 4
#define OCTARYN_CLIENT_CHUNK_VIEW_MAX_WIDTH 34

typedef struct octaryn_client_chunk_view
{
    int origin_x;
    int origin_z;
    int width;
}
octaryn_client_chunk_view;

int octaryn_client_chunk_origin_for_position(float position, int view_width);
octaryn_client_chunk_view octaryn_client_chunk_view_for_camera(
    float camera_x,
    float camera_z,
    int render_distance);
int octaryn_client_chunk_view_equal(
    const octaryn_client_chunk_view* left,
    const octaryn_client_chunk_view* right);

#ifdef __cplusplus
}
#endif
