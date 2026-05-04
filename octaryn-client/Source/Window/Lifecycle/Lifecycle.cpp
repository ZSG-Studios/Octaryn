#include "Lifecycle.h"

#include "FullscreenDisplayMode.h"

namespace {

auto display_for_window(SDL_Window* window) -> SDL_DisplayID
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }

    return display;
}

auto set_best_fullscreen_mode(SDL_Window* window) -> int
{
    SDL_DisplayMode mode{};
    if (!fullscreen_display_mode_best(display_for_window(window), &mode))
    {
        return 0;
    }

    return SDL_SetWindowFullscreenMode(window, &mode) ? 1 : 0;
}

} // namespace

int window_lifecycle_toggle_fullscreen(SDL_Window* window)
{
    if (window == nullptr)
    {
        return 0;
    }

    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    const bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    const bool enter_fullscreen = !is_fullscreen;
    if (enter_fullscreen && !set_best_fullscreen_mode(window))
    {
        return 0;
    }

    if (!SDL_SetWindowFullscreen(window, enter_fullscreen))
    {
        return 0;
    }

    SDL_SyncWindow(window);
    SDL_SetWindowRelativeMouseMode(window, enter_fullscreen);
    return 1;
}

int window_lifecycle_apply_best_fullscreen(SDL_Window* window)
{
    if (window == nullptr || !set_best_fullscreen_mode(window))
    {
        return 0;
    }

    return SDL_SetWindowFullscreen(window, true) ? 1 : 0;
}

int window_lifecycle_show(SDL_Window* window)
{
    if (window == nullptr)
    {
        return 0;
    }

    SDL_SetWindowAlwaysOnTop(window, true);
    if (!SDL_ShowWindow(window))
    {
        return 0;
    }

    SDL_RaiseWindow(window);
    SDL_SyncWindow(window);
    return 1;
}

void window_lifecycle_finish_show(SDL_Window* window)
{
    if (window != nullptr)
    {
        SDL_SetWindowAlwaysOnTop(window, false);
    }
}
