#include "internal.h"

namespace app_window_internal {

bool get_fullscreen_mode(SDL_DisplayID display, SDL_DisplayMode* mode)
{
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (!modes || count <= 0)
    {
        const SDL_DisplayMode* current_mode = SDL_GetCurrentDisplayMode(display);
        if (!current_mode)
        {
            SDL_Log("Failed to get display mode: %s", SDL_GetError());
            return false;
        }
        *mode = *current_mode;
        return true;
    }
    int best_index = 0;
    float best_pixels = 0.0f;
    float best_refresh = 0.0f;
    for (int i = 0; i < count; i++)
    {
        const SDL_DisplayMode* candidate = modes[i];
        const float pixel_width = static_cast<float>(candidate->w) * candidate->pixel_density;
        const float pixel_height = static_cast<float>(candidate->h) * candidate->pixel_density;
        float pixels = pixel_width * pixel_height;
        if (pixels > best_pixels || (pixels == best_pixels && candidate->refresh_rate > best_refresh))
        {
            best_index = i;
            best_pixels = pixels;
            best_refresh = candidate->refresh_rate;
        }
    }
    *mode = *modes[best_index];
    SDL_free(modes);
    return true;
}

} // namespace app_window_internal

bool main_window_toggle_fullscreen(SDL_Window* window)
{
    const bool is_fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!is_fullscreen)
    {
        SDL_DisplayID display = SDL_GetDisplayForWindow(window);
        if (!display)
        {
            display = SDL_GetPrimaryDisplay();
        }
        SDL_DisplayMode mode = {};
        if (!app_window_internal::get_fullscreen_mode(display, &mode))
        {
            return false;
        }
        if (!SDL_SetWindowFullscreenMode(window, &mode))
        {
            SDL_Log("Failed to set fullscreen mode: %s", SDL_GetError());
            return false;
        }
        SDL_Log("Using fullscreen mode: %dx%d @ %.2fHz (density %.2f)", mode.w, mode.h, mode.refresh_rate, mode.pixel_density);
    }
    if (!SDL_SetWindowFullscreen(window, !is_fullscreen))
    {
        SDL_Log("Failed to toggle fullscreen: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SyncWindow(window))
    {
        SDL_Log("Failed to sync window state: %s", SDL_GetError());
    }
    SDL_SetWindowRelativeMouseMode(window, !is_fullscreen);
    return true;
}

bool main_window_apply_best_fullscreen(SDL_Window* window)
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display)
    {
        display = SDL_GetPrimaryDisplay();
    }
    SDL_DisplayMode mode = {};
    if (!app_window_internal::get_fullscreen_mode(display, &mode))
    {
        return false;
    }
    if (!SDL_SetWindowFullscreenMode(window, &mode))
    {
        SDL_Log("Failed to set fullscreen mode: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetWindowFullscreen(window, true))
    {
        SDL_Log("Failed to enter fullscreen: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool main_window_show(SDL_Window* window)
{
    SDL_SetWindowAlwaysOnTop(window, true);
    if (!SDL_ShowWindow(window))
    {
        SDL_Log("Failed to show window: %s", SDL_GetError());
        return false;
    }
    SDL_RaiseWindow(window);
    if (!SDL_SyncWindow(window))
    {
        SDL_Log("Failed to sync shown window: %s", SDL_GetError());
    }
    return true;
}

void main_window_finish_show(SDL_Window* window)
{
    SDL_SetWindowAlwaysOnTop(window, false);
}
