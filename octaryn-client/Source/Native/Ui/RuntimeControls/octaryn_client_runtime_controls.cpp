#include "octaryn_client_runtime_controls.h"

#include "octaryn_client_runtime_controls_menu.h"

#if defined(OCTARYN_CLIENT_RUNTIME_CONTROLS_USE_SDL3)

void octaryn_client_runtime_controls_init(octaryn_client_runtime_controls* controls)
{
    if (controls == nullptr)
    {
        return;
    }

    *controls = {};
    controls->debug_overlay_enabled = 1u;
    controls->fog_enabled = 1u;
    controls->clouds_enabled = 1u;
    controls->sky_gradient_enabled = 1u;
    controls->stars_enabled = 1u;
    controls->sun_enabled = 1u;
    controls->moon_enabled = 1u;
    controls->pom_enabled = 1u;
    controls->pbr_enabled = 1u;
    controls->present_mode_index = 0;
    controls->render_distance = 32;
}

uint8_t octaryn_client_runtime_controls_ui_active(const octaryn_client_runtime_controls* controls)
{
    return controls != nullptr && controls->display_menu.active != 0u ? 1u : 0u;
}

void octaryn_client_runtime_controls_refresh_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height)
{
    if (controls == nullptr)
    {
        return;
    }

    display_catalog_refresh(
        &controls->display_catalog,
        window,
        viewport_width,
        viewport_height);
    octaryn_client_runtime_controls_copy_to_menu(controls, window);
}

void octaryn_client_runtime_controls_sync_relative_mouse(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window)
{
    if (controls == nullptr || window == nullptr)
    {
        return;
    }

    if (octaryn_client_runtime_controls_ui_active(controls) != 0u)
    {
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            controls->restore_relative_mouse_after_ui = 1u;
            SDL_SetWindowRelativeMouseMode(window, false);
        }
        return;
    }

    if (controls->restore_relative_mouse_after_ui != 0u)
    {
        SDL_SetWindowRelativeMouseMode(window, true);
        controls->restore_relative_mouse_after_ui = 0u;
    }
}

#endif
