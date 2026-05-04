#include "RuntimeControls.h"

#if defined(RUNTIME_CONTROLS_USE_SDL3)

#include "octaryn_client_render_distance.h"
#include "Menu.h"
#include "Lifecycle.h"

uint32_t runtime_controls_handle_event(
    runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event,
    int32_t viewport_width,
    int32_t viewport_height)
{
    if (controls == nullptr || window == nullptr || event == nullptr)
    {
        return 0u;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        if (event->key.scancode == SDL_SCANCODE_F11)
        {
            window_lifecycle_toggle_fullscreen(window);
            runtime_controls_refresh_menu(controls, window, viewport_width, viewport_height);
            runtime_controls_sync_relative_mouse(controls, window);
            return RUNTIME_CONTROLS_EVENT_CAPTURED |
                RUNTIME_CONTROLS_FULLSCREEN_TOGGLED;
        }
        if (event->key.scancode == SDL_SCANCODE_F3)
        {
            controls->debug_overlay_enabled = controls->debug_overlay_enabled == 0u ? 1u : 0u;
            return RUNTIME_CONTROLS_EVENT_CAPTURED |
                RUNTIME_CONTROLS_DEBUG_TOGGLED;
        }
    }

    if (runtime_controls_ui_active(controls) != 0u)
    {
        if (event->type == SDL_EVENT_MOUSE_MOTION)
        {
            const int32_t row = runtime_controls_hit_menu_row(
                window,
                viewport_width,
                viewport_height,
                event->motion.x,
                event->motion.y);
            if (row >= 0)
            {
                controls->display_menu.row = row;
            }
            return RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
            {
                const int32_t row = runtime_controls_hit_menu_row(
                    window,
                    viewport_width,
                    viewport_height,
                    event->button.x,
                    event->button.y);
                return runtime_controls_activate_menu_row(
                    controls,
                    window,
                    row,
                    event->button.button == SDL_BUTTON_LEFT ? 1 : -1);
            }
            return RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_MOUSE_WHEEL)
        {
            if (event->wheel.y > 0.0f)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + DISPLAY_MENU_ROW_COUNT - 1) %
                    DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->wheel.y < 0.0f)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + 1) % DISPLAY_MENU_ROW_COUNT;
            }
            return RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
        {
            if (event->key.scancode == SDL_SCANCODE_ESCAPE)
            {
                return RUNTIME_CONTROLS_EVENT_CAPTURED |
                    runtime_controls_close_menu(controls, window);
            }
            if (event->key.scancode == SDL_SCANCODE_UP)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + DISPLAY_MENU_ROW_COUNT - 1) %
                    DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_DOWN)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + 1) % DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_LEFT)
            {
                display_menu_adjust(
                    &controls->display_menu,
                    -1,
                    octaryn_client_render_distance_option_count());
            }
            else if (event->key.scancode == SDL_SCANCODE_RIGHT)
            {
                display_menu_adjust(
                    &controls->display_menu,
                    1,
                    octaryn_client_render_distance_option_count());
            }
            else if (event->key.scancode == SDL_SCANCODE_RETURN ||
                     event->key.scancode == SDL_SCANCODE_KP_ENTER)
            {
                return runtime_controls_activate_menu_row(
                    controls,
                    window,
                    controls->display_menu.row,
                    1);
            }
            return RUNTIME_CONTROLS_EVENT_CAPTURED;
        }

        return 0u;
    }

    if (event->type == SDL_EVENT_KEY_DOWN &&
        !event->key.repeat &&
        event->key.scancode == SDL_SCANCODE_ESCAPE)
    {
        runtime_controls_refresh_menu(controls, window, viewport_width, viewport_height);
        display_menu_open(&controls->display_menu);
        runtime_controls_sync_relative_mouse(controls, window);
        return RUNTIME_CONTROLS_EVENT_CAPTURED |
            RUNTIME_CONTROLS_MENU_OPENED;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        !SDL_GetWindowRelativeMouseMode(window))
    {
        SDL_SetWindowRelativeMouseMode(window, true);
        controls->restore_relative_mouse_after_ui = 0u;
        return RUNTIME_CONTROLS_EVENT_CAPTURED;
    }

    return 0u;
}

#endif
