#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_profile.h"

void app_configure_sdl_logging(void);
Uint64 app_profile_now(void);
float app_profile_elapsed_ms(Uint64 start_ticks);
void app_maybe_log_profile_spike(const main_frame_profile_sample_t* sample);
