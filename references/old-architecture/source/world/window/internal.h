#pragma once

typedef struct camera camera_t;

auto world_window_origin_for_position(float position) -> int;
auto world_window_origin_for_position_with_width(float position, int active_world_width) -> int;
void world_window_reframe_internal(int origin_x, int origin_z, int active_world_width);
void world_window_reset_at_origin_internal(int origin_x, int origin_z);
void world_window_reset_internal(const camera_t* camera);
void world_window_move_chunks_internal(const camera_t* camera);
void world_window_queue_move_internal(int origin_x, int origin_z);
void world_window_queue_render_distance_internal(const camera_t* camera, int chunk_distance);
void world_window_queue_reset_internal(const camera_t* camera);
bool world_window_transition_pending_internal(void);
bool world_window_try_apply_pending_transition_internal(void);
