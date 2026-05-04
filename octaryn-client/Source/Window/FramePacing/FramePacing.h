#pragma once

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum present_mode_policy
{
    PRESENT_MODE_POLICY_AUTO = 0,
    PRESENT_MODE_POLICY_IMMEDIATE = 1,
    PRESENT_MODE_POLICY_MAILBOX = 2,
    PRESENT_MODE_POLICY_VSYNC = 3,
}
present_mode_policy;

typedef enum swapchain_acquire_mode
{
    SWAPCHAIN_ACQUIRE_EARLY = 0,
    SWAPCHAIN_ACQUIRE_LATE = 1,
    SWAPCHAIN_ACQUIRE_NONBLOCKING = 2,
}
swapchain_acquire_mode;

typedef struct frame_pacing
{
    present_mode_policy requested_present_mode;
    SDL_GPUPresentMode actual_present_mode;
    swapchain_acquire_mode acquire_mode;
    int fps_cap;
    int fps_cap_spin_us;
    int allowed_frames_in_flight;
    int swapchain_unavailable_sleep_us;
    Uint64 next_frame_target_ticks;
}
frame_pacing;

void frame_pacing_init(frame_pacing* state);
void frame_pacing_set_actual_present_mode(
    frame_pacing* state,
    SDL_GPUPresentMode present_mode);
SDL_GPUPresentMode frame_pacing_choose_present_mode(
    const frame_pacing* state,
    SDL_GPUDevice* device,
    SDL_Window* window);
int frame_pacing_should_defer_swapchain_acquire(
    const frame_pacing* state);
int frame_pacing_should_probe_swapchain_before_scene(
    const frame_pacing* state);
float frame_pacing_sleep_until_next_frame(
    frame_pacing* state,
    Uint64 frame_start_ticks);
float frame_pacing_sleep_after_swapchain_unavailable(
    const frame_pacing* state);
const char* frame_pacing_present_policy_name(
    present_mode_policy policy);
const char* frame_pacing_acquire_mode_name(
    swapchain_acquire_mode mode);

#ifdef __cplusplus
}
#endif
