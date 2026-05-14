#pragma once

void app_prepare_startup_terrain(void);
int app_render_distance_setting(int live_render_distance);
void app_set_render_distance_setting(int requested_render_distance, int live_render_distance);
void app_reset_startup_terrain_state(void);
float app_spawn_eye_height(void);
void app_log_world_debug_snapshot(const char* reason);
void app_fallback_to_default_spawn(const char* reason);
void app_maybe_align_spawn_to_surface(void);
void app_maybe_log_terrain_debug(void);
void app_maybe_expand_startup_render_distance(void);
