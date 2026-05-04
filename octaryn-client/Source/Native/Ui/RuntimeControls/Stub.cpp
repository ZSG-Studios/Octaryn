#include "RuntimeControls.h"

#if !defined(RUNTIME_CONTROLS_USE_SDL3)

void runtime_controls_init(runtime_controls* controls)
{
    if (controls != nullptr)
    {
        *controls = {};
    }
}

uint8_t runtime_controls_ui_active(const runtime_controls* controls)
{
    (void)controls;
    return 0u;
}

void runtime_controls_refresh_menu(
    runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height)
{
    (void)controls;
    (void)window;
    (void)viewport_width;
    (void)viewport_height;
}

void runtime_controls_sync_relative_mouse(
    runtime_controls* controls,
    SDL_Window* window)
{
    (void)controls;
    (void)window;
}

uint32_t runtime_controls_handle_event(
    runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event,
    int32_t viewport_width,
    int32_t viewport_height)
{
    (void)controls;
    (void)window;
    (void)event;
    (void)viewport_width;
    (void)viewport_height;
    return 0u;
}

#endif
