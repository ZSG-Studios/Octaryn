#include "app/runtime/events.h"

#include "app/overlay/display_menu.h"
#include "app/overlay/interaction.h"
#include "app/runtime/internal.h"
#include "app/runtime/launch.h"
#include "core/log.h"

namespace {

bool app_display_menu_accept_key(const SDL_KeyboardEvent& key)
{
    return key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER ||
           key.key == SDLK_RETURN || key.key == SDLK_KP_ENTER;
}

int app_display_menu_hit_row(float x, float y)
{
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height))
    {
        width = player.camera.viewport_size[0];
        height = player.camera.viewport_size[1];
    }
    return main_display_menu_hit_row(width, height, x, y);
}

SDL_AppResult app_activate_display_menu_row(int row, int delta)
{
    if (row < 0 || row >= MAIN_DISPLAY_MENU_ROW_COUNT)
    {
        return SDL_APP_CONTINUE;
    }
    display_menu.row = row;
    if (display_menu.row == MAIN_DISPLAY_MENU_APPLY_ROW)
    {
        main_display_menu_request_apply(&display_menu);
    }
    else if (display_menu.row == MAIN_DISPLAY_MENU_CLOSE_ROW)
    {
        app_close_display_menu();
    }
    else if (display_menu.row == MAIN_DISPLAY_MENU_EXIT_ROW)
    {
        app_apply_display_menu_settings();
        quit_requested = true;
        return SDL_APP_SUCCESS;
    }
    else
    {
        main_display_menu_adjust(&display_menu, app_get_display_menu_runtime(), delta);
    }
    return SDL_APP_CONTINUE;
}

} // namespace

SDL_AppResult app_runtime_event(void* appstate, SDL_Event* event)
{
    (void) appstate;

    main_imgui_lighting_process_event(event);
    if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        oct_log_infof("Got event %d", event->type);
    }
    switch (event->type)
    {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        oct_log_infof("Got SDL_EVENT_WINDOW_CLOSE_REQUESTED");
        if (app_should_ignore_early_close())
        {
            oct_log_warnf("Ignoring early SDL_EVENT_WINDOW_CLOSE_REQUESTED during detached launch startup");
            return SDL_APP_CONTINUE;
        }
        oct_log_warnf("Received SDL_EVENT_WINDOW_CLOSE_REQUESTED");
        quit_requested = true;
        return SDL_APP_SUCCESS;
    case SDL_EVENT_QUIT:
        oct_log_infof("Ignoring SDL_EVENT_QUIT");
        return SDL_APP_CONTINUE;
    case SDL_EVENT_MOUSE_MOTION:
        if (display_menu.active)
        {
            const int row = app_display_menu_hit_row(event->motion.x, event->motion.y);
            if (row >= 0)
            {
                display_menu.row = row;
            }
            break;
        }
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            player_rotate(&player, event->motion.yrel, event->motion.xrel);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat)
        {
            break;
        }
        if (app_handle_global_keydown(event->key.scancode))
        {
            break;
        }
        if (lighting_tuning.visible && main_imgui_lighting_wants_capture())
        {
            break;
        }
        if (display_menu.active)
        {
            if (event->key.scancode == SDL_SCANCODE_ESCAPE)
            {
                app_close_display_menu();
            }
            else if (event->key.scancode == SDL_SCANCODE_UP)
            {
                display_menu.row = (display_menu.row + MAIN_DISPLAY_MENU_ROW_COUNT - 1) % MAIN_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_DOWN)
            {
                display_menu.row = (display_menu.row + 1) % MAIN_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_LEFT)
            {
                main_display_menu_adjust(&display_menu, app_get_display_menu_runtime(), -1);
            }
            else if (event->key.scancode == SDL_SCANCODE_RIGHT)
            {
                main_display_menu_adjust(&display_menu, app_get_display_menu_runtime(), 1);
            }
            else if (app_display_menu_accept_key(event->key))
            {
                if (display_menu.row == MAIN_DISPLAY_MENU_APPLY_ROW)
                {
                    main_display_menu_request_apply(&display_menu);
                }
                else if (display_menu.row == MAIN_DISPLAY_MENU_CLOSE_ROW)
                {
                    app_close_display_menu();
                }
                else if (display_menu.row == MAIN_DISPLAY_MENU_EXIT_ROW)
                {
                    app_apply_display_menu_settings();
                    quit_requested = true;
                    return SDL_APP_SUCCESS;
                }
                else
                {
                    main_display_menu_adjust(&display_menu, app_get_display_menu_runtime(), 1);
                }
            }
            break;
        }
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
        {
            app_open_display_menu();
        }
        else if (event->key.scancode == SDL_SCANCODE_F5 || event->key.scancode == SDL_SCANCODE_F)
        {
            player_toggle_controller(&player);
        }
        else if (event->key.scancode == SDL_SCANCODE_Z)
        {
            camera_cycle_zoom(&player.camera);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (display_menu.active)
        {
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                const int row = app_display_menu_hit_row(event->button.x, event->button.y);
                SDL_AppResult result = app_activate_display_menu_row(row, 1);
                if (result != SDL_APP_CONTINUE)
                {
                    return result;
                }
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
            {
                const int row = app_display_menu_hit_row(event->button.x, event->button.y);
                SDL_AppResult result = app_activate_display_menu_row(row, -1);
                if (result != SDL_APP_CONTINUE)
                {
                    return result;
                }
            }
            break;
        }
        if (lighting_tuning.visible && main_imgui_lighting_wants_capture())
        {
            break;
        }
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_SetWindowRelativeMouseMode(window, true);
            restore_relative_mouse_after_ui = false;
        }
        else
        {
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                player_break_block(&player);
            }
            else if (event->button.button == SDL_BUTTON_MIDDLE)
            {
                player_select_block(&player);
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
            {
                player_place_block(&player);
            }
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (display_menu.active)
        {
            if (event->wheel.y > 0.0f)
            {
                display_menu.row = (display_menu.row + MAIN_DISPLAY_MENU_ROW_COUNT - 1) % MAIN_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->wheel.y < 0.0f)
            {
                display_menu.row = (display_menu.row + 1) % MAIN_DISPLAY_MENU_ROW_COUNT;
            }
            break;
        }
        if (lighting_tuning.visible && main_imgui_lighting_wants_capture())
        {
            break;
        }
        player_change_block(&player, (int) event->wheel.y);
        break;
    }

    return SDL_APP_CONTINUE;
}
