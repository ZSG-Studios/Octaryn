#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CHUNK_VIEW_CHUNK_WIDTH 32
#define CHUNK_VIEW_COLUMN_HEIGHT_CHUNKS 32
#define CHUNK_VIEW_WORLD_HEIGHT_BLOCKS \
    (CHUNK_VIEW_CHUNK_WIDTH * CHUNK_VIEW_COLUMN_HEIGHT_CHUNKS)
#define CHUNK_VIEW_MIN_WIDTH 4
#define CHUNK_VIEW_TARGET_MAX_WIDTH 257
#define CHUNK_VIEW_MAX_WIDTH CHUNK_VIEW_TARGET_MAX_WIDTH

typedef struct chunk_view
{
    int origin_x;
    int origin_z;
    int width;
}
chunk_view;

int chunk_origin_for_position(float position, int view_width);
chunk_view chunk_view_for_camera(
    float camera_x,
    float camera_z,
    int render_distance);
int chunk_view_equal(
    const chunk_view* left,
    const chunk_view* right);

#ifdef __cplusplus
}
#endif
