#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int render_distance_option_count(void);
const int* render_distance_options(void);
int render_distance_sanitize(int distance);
int render_distance_next_step(int current_distance, int target_distance);

#ifdef __cplusplus
}
#endif
