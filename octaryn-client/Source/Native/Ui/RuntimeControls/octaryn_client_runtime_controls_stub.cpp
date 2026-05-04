#include "octaryn_client_runtime_controls.h"

#if !defined(OCTARYN_CLIENT_RUNTIME_CONTROLS_USE_SDL3)

void octaryn_client_runtime_controls_init(octaryn_client_runtime_controls* controls)
{
    if (controls != nullptr)
    {
        *controls = {};
    }
}

uint8_t octaryn_client_runtime_controls_ui_active(const octaryn_client_runtime_controls* controls)
{
    (void)controls;
    return 0u;
}

void octaryn_client_runtime_controls_refresh_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height)
{
    (void)controls;
    (void)window;
    (void)viewport_width;
    (void)viewport_height;
}

void octaryn_client_runtime_controls_sync_relative_mouse(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window)
{
    (void)controls;
    (void)window;
}

uint32_t octaryn_client_runtime_controls_handle_event(
    octaryn_client_runtime_controls* controls,
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
