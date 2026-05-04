#include "Swapchain.h"

void swapchain_state_init(swapchain_state* state)
{
    if (state != nullptr)
    {
        state->present_mode = SDL_GPU_PRESENTMODE_VSYNC;
    }
}

int swapchain_configure(
    swapchain_state* state,
    SDL_GPUDevice* device,
    SDL_Window* window,
    frame_pacing* frame_pacing)
{
    const SDL_GPUPresentMode present_mode =
        frame_pacing_choose_present_mode(frame_pacing, device, window);
    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode))
    {
        return 0;
    }

    const int frames_in_flight = frame_pacing != nullptr ? frame_pacing->allowed_frames_in_flight : 3;
    if (!SDL_SetGPUAllowedFramesInFlight(device, static_cast<Uint32>(frames_in_flight)))
    {
        return 0;
    }

    if (state != nullptr)
    {
        state->present_mode = present_mode;
    }
    frame_pacing_set_actual_present_mode(frame_pacing, present_mode);
    return 1;
}

const char* swapchain_present_mode_name(
    const swapchain_state* state)
{
    const SDL_GPUPresentMode present_mode =
        state != nullptr ? state->present_mode : SDL_GPU_PRESENTMODE_VSYNC;
    return swapchain_present_mode_value_name(present_mode);
}

const char* swapchain_present_mode_value_name(
    SDL_GPUPresentMode present_mode)
{
    switch (present_mode)
    {
    case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate";
    case SDL_GPU_PRESENTMODE_MAILBOX: return "mailbox";
    case SDL_GPU_PRESENTMODE_VSYNC: return "vsync";
    default: return "unknown";
    }
}
