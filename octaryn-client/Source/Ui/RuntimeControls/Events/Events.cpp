#include "RuntimeControls.h"

#if defined(RUNTIME_CONTROLS_USE_SDL3)

namespace {

uint32_t handle_key(runtime_controls* controls, SDL_Window* window, const SDL_KeyboardEvent& key)
{
    if (key.down && !key.repeat)
    {
        if (key.scancode == SDL_SCANCODE_F11)
        {
            const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0u;
            SDL_SetWindowFullscreen(window, fullscreen);
            return RUNTIME_CONTROLS_FULLSCREEN_TOGGLED;
        }
        if (key.scancode == SDL_SCANCODE_F3)
        {
            controls->debug_overlay_enabled = !controls->debug_overlay_enabled;
            return RUNTIME_CONTROLS_DEBUG_TOGGLED;
        }
        if (key.scancode == SDL_SCANCODE_F || key.scancode == SDL_SCANCODE_F5)
        {
            return RUNTIME_CONTROLS_FLIGHT_TOGGLED;
        }
        if (key.scancode == SDL_SCANCODE_Z)
        {
            return RUNTIME_CONTROLS_ZOOM_CYCLED;
        }
    }
    return 0u;
}

} // namespace

uint32_t runtime_controls_handle_event(
    runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event)
{
    if (controls == nullptr || event == nullptr || window == nullptr)
    {
        return 0u;
    }
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
    {
        return handle_key(controls, window, event->key);
    }
    return 0u;
}

#endif
