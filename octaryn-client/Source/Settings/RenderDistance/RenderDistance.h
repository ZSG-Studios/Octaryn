#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_DISTANCE_MIN_CHUNKS 4
#define RENDER_DISTANCE_DEFAULT_CHUNKS 4
#define RENDER_DISTANCE_TARGET_MAX_CHUNKS 128
#define RENDER_DISTANCE_MAX_CHUNKS RENDER_DISTANCE_TARGET_MAX_CHUNKS
#define RENDER_DISTANCE_STEP_CHUNKS 4

int render_distance_option_count(void);
const int* render_distance_options(void);
int render_distance_sanitize(int distance);
int render_distance_next_step(int current_distance, int target_distance);

#ifdef __cplusplus
}
#endif
