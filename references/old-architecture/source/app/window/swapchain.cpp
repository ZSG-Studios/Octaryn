#include "internal.h"

namespace app_window_internal {

auto get_present_mode_name(SDL_GPUPresentMode present_mode) -> const char*
{
    switch (present_mode)
    {
    case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate";
    case SDL_GPU_PRESENTMODE_MAILBOX: return "mailbox";
    case SDL_GPU_PRESENTMODE_VSYNC: return "vsync";
    default: return "unknown";
    }
}

} // namespace app_window_internal

bool main_window_configure_swapchain(main_window_t* state,
                                     SDL_GPUDevice* device,
                                     SDL_Window* window,
                                     app_frame_pacing_t* frame_pacing)
{
    SDL_GPUPresentMode present_mode = app_frame_pacing_choose_present_mode(frame_pacing, device, window);
    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode))
    {
        SDL_Log("Failed to set swapchain parameters: %s", SDL_GetError());
        return false;
    }
    const int frames_in_flight = frame_pacing ? frame_pacing->allowed_frames_in_flight : 3;
    if (!SDL_SetGPUAllowedFramesInFlight(device, static_cast<Uint32>(frames_in_flight)))
    {
        SDL_Log("Failed to set allowed frames in flight: %s", SDL_GetError());
        return false;
    }
    if (state)
    {
        state->present_mode = present_mode;
    }
    app_frame_pacing_set_actual_present_mode(frame_pacing, present_mode);
    SDL_Log("Using GPU present mode: %s (frames_in_flight=%d)",
            app_window_internal::get_present_mode_name(present_mode),
            frames_in_flight);
    return true;
}

const char* main_window_present_mode_name(const main_window_t* state)
{
    const SDL_GPUPresentMode present_mode = state ? state->present_mode : SDL_GPU_PRESENTMODE_VSYNC;
    return app_window_internal::get_present_mode_name(present_mode);
}
