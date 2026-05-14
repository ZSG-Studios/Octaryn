#pragma once

#include <SDL3/SDL.h>

typedef enum app_present_mode_policy
{
    APP_PRESENT_MODE_POLICY_AUTO,
    APP_PRESENT_MODE_POLICY_IMMEDIATE,
    APP_PRESENT_MODE_POLICY_MAILBOX,
    APP_PRESENT_MODE_POLICY_VSYNC,
}
app_present_mode_policy_t;

typedef enum app_swapchain_acquire_mode
{
    APP_SWAPCHAIN_ACQUIRE_EARLY,
    APP_SWAPCHAIN_ACQUIRE_LATE,
    APP_SWAPCHAIN_ACQUIRE_NONBLOCKING,
}
app_swapchain_acquire_mode_t;

typedef struct app_frame_pacing
{
    app_present_mode_policy_t requested_present_mode;
    SDL_GPUPresentMode actual_present_mode;
    app_swapchain_acquire_mode_t acquire_mode;
    int fps_cap;
    int fps_cap_spin_us;
    int allowed_frames_in_flight;
    int swapchain_unavailable_sleep_us;
    Uint64 next_frame_target_ticks;
}
app_frame_pacing_t;

void app_frame_pacing_init(app_frame_pacing_t* state);
void app_frame_pacing_set_actual_present_mode(app_frame_pacing_t* state, SDL_GPUPresentMode present_mode);
SDL_GPUPresentMode app_frame_pacing_choose_present_mode(const app_frame_pacing_t* state,
                                                        SDL_GPUDevice* device,
                                                        SDL_Window* window);
bool app_frame_pacing_should_defer_swapchain_acquire(const app_frame_pacing_t* state);
bool app_frame_pacing_should_probe_swapchain_before_scene(const app_frame_pacing_t* state);
float app_frame_pacing_sleep_until_next_frame(app_frame_pacing_t* state, Uint64 frame_start_ticks);
float app_frame_pacing_sleep_after_swapchain_unavailable(const app_frame_pacing_t* state);
const char* app_frame_pacing_present_policy_name(app_present_mode_policy_t policy);
const char* app_frame_pacing_acquire_mode_name(app_swapchain_acquire_mode_t mode);
