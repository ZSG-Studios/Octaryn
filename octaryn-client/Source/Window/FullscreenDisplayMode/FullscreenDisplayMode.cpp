#include "FullscreenDisplayMode.h"

#if defined(OCTARYN_CLIENT_FULLSCREEN_DISPLAY_MODE_USE_SDL3)

namespace {

auto physical_pixels(const SDL_DisplayMode* mode) -> float
{
    if (mode == nullptr || mode->w <= 0 || mode->h <= 0 || mode->pixel_density <= 0.0f)
    {
        return 0.0f;
    }

    const float pixel_width = static_cast<float>(mode->w) * mode->pixel_density;
    const float pixel_height = static_cast<float>(mode->h) * mode->pixel_density;
    return pixel_width * pixel_height;
}

auto copy_current_mode(SDL_DisplayID display, SDL_DisplayMode* mode) -> int
{
    const SDL_DisplayMode* current_mode = SDL_GetCurrentDisplayMode(display);
    if (current_mode == nullptr)
    {
        return 0;
    }

    *mode = *current_mode;
    return 1;
}

} // namespace

#endif

int fullscreen_display_mode_best(
    SDL_DisplayID display,
    SDL_DisplayMode* mode)
{
    if (mode == nullptr)
    {
        return 0;
    }

#if defined(OCTARYN_CLIENT_FULLSCREEN_DISPLAY_MODE_USE_SDL3)
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (modes == nullptr || count <= 0)
    {
        if (modes != nullptr)
        {
            SDL_free(modes);
        }
        return copy_current_mode(display, mode);
    }

    int best_index = 0;
    float best_pixels = 0.0f;
    float best_refresh = 0.0f;
    for (int index = 0; index < count; ++index)
    {
        const SDL_DisplayMode* candidate = modes[index];
        const float pixels = physical_pixels(candidate);
        const float refresh_rate = candidate != nullptr ? candidate->refresh_rate : 0.0f;
        if (pixels > best_pixels || (pixels == best_pixels && refresh_rate > best_refresh))
        {
            best_index = index;
            best_pixels = pixels;
            best_refresh = refresh_rate;
        }
    }

    if (modes[best_index] == nullptr)
    {
        SDL_free(modes);
        return copy_current_mode(display, mode);
    }

    *mode = *modes[best_index];
    SDL_free(modes);
    return 1;
#else
    (void)display;
    return 0;
#endif
}
