#include "RuntimeControls.h"

#if defined(RUNTIME_CONTROLS_USE_SDL3)

#include "RenderDistance.h"
#include "Menu.h"
#include "Lifecycle.h"

namespace {

auto append_menu_char(char* text, int capacity, char value) -> void
{
    if (text == nullptr || capacity <= 1)
    {
        return;
    }
    int length = 0;
    while (length < capacity - 1 && text[length] != '\0')
    {
        ++length;
    }
    if (length < capacity - 1)
    {
        text[length] = value;
        text[length + 1] = '\0';
    }
}

auto pop_menu_char(char* text, int capacity) -> void
{
    if (text == nullptr || capacity <= 1)
    {
        return;
    }
    int length = 0;
    while (length < capacity - 1 && text[length] != '\0')
    {
        ++length;
    }
    if (length > 0)
    {
        text[length - 1] = '\0';
    }
}

auto key_to_menu_char(SDL_Scancode scancode, bool port_only) -> char
{
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
    {
        return static_cast<char>('1' + (scancode - SDL_SCANCODE_1));
    }
    if (scancode == SDL_SCANCODE_0)
    {
        return '0';
    }
    if (!port_only && scancode == SDL_SCANCODE_PERIOD)
    {
        return '.';
    }
    return '\0';
}

auto key_to_world_name_char(SDL_Scancode scancode) -> char
{
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
    {
        return static_cast<char>('A' + (scancode - SDL_SCANCODE_A));
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
    {
        return static_cast<char>('1' + (scancode - SDL_SCANCODE_1));
    }
    if (scancode == SDL_SCANCODE_0)
    {
        return '0';
    }
    if (scancode == SDL_SCANCODE_SPACE)
    {
        return ' ';
    }
    return '\0';
}

auto handle_multiplayer_text_key(display_menu& menu, SDL_Scancode scancode) -> bool
{
    if (menu.screen != DISPLAY_MENU_SCREEN_MULTIPLAYER ||
        (menu.editing_field != 1u && menu.editing_field != 2u))
    {
        return false;
    }
    char* text = menu.editing_field == 1u ? menu.server_address : menu.server_port;
    const int capacity = menu.editing_field == 1u
        ? DISPLAY_MENU_SERVER_ADDRESS_SIZE
        : DISPLAY_MENU_SERVER_PORT_SIZE;
    if (scancode == SDL_SCANCODE_BACKSPACE)
    {
        pop_menu_char(text, capacity);
        return true;
    }
    const char value = key_to_menu_char(scancode, menu.editing_field == 2u);
    if (value != '\0')
    {
        append_menu_char(text, capacity, value);
        return true;
    }
    return false;
}

auto handle_world_name_key(display_menu& menu, SDL_Scancode scancode) -> bool
{
    if (menu.screen != DISPLAY_MENU_SCREEN_SINGLEPLAYER ||
        menu.editing_field != 3u)
    {
        return false;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE)
    {
        pop_menu_char(menu.world_name, DISPLAY_MENU_WORLD_NAME_SIZE);
        return true;
    }
    const char value = key_to_world_name_char(scancode);
    if (value != '\0')
    {
        append_menu_char(menu.world_name, DISPLAY_MENU_WORLD_NAME_SIZE, value);
        return true;
    }
    return false;
}

} // namespace

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
                if (display_menu_row_selectable(&controls->display_menu, row) != 0u)
                {
                    controls->display_menu.row = row;
                }
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
                display_menu_move_row(&controls->display_menu, -1);
            }
            else if (event->wheel.y < 0.0f)
            {
                display_menu_move_row(&controls->display_menu, 1);
            }
            return RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
        {
            if (handle_multiplayer_text_key(
                    controls->display_menu,
                    event->key.scancode))
            {
                return RUNTIME_CONTROLS_EVENT_CAPTURED;
            }
            if (handle_world_name_key(
                    controls->display_menu,
                    event->key.scancode))
            {
                return RUNTIME_CONTROLS_EVENT_CAPTURED;
            }
            if (event->key.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (controls->display_menu.screen == DISPLAY_MENU_SCREEN_SINGLEPLAYER ||
                    controls->display_menu.screen == DISPLAY_MENU_SCREEN_MULTIPLAYER ||
                    controls->display_menu.screen == DISPLAY_MENU_SCREEN_INGAME)
                {
                    controls->display_menu.screen = controls->session_active != 0u
                        ? DISPLAY_MENU_SCREEN_INGAME
                        : DISPLAY_MENU_SCREEN_MAIN;
                    controls->display_menu.row = 2;
                    return RUNTIME_CONTROLS_EVENT_CAPTURED;
                }
                return RUNTIME_CONTROLS_EVENT_CAPTURED |
                    runtime_controls_close_menu(controls, window);
            }
            if (event->key.scancode == SDL_SCANCODE_UP)
            {
                display_menu_move_row(&controls->display_menu, -1);
            }
            else if (event->key.scancode == SDL_SCANCODE_DOWN)
            {
                display_menu_move_row(&controls->display_menu, 1);
            }
            else if (event->key.scancode == SDL_SCANCODE_LEFT)
            {
                display_menu_adjust(
                    &controls->display_menu,
                    -1,
                    render_distance_option_count());
            }
            else if (event->key.scancode == SDL_SCANCODE_RIGHT)
            {
                display_menu_adjust(
                    &controls->display_menu,
                    1,
                    render_distance_option_count());
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
        if (controls->session_active != 0u)
        {
            controls->display_menu.screen = DISPLAY_MENU_SCREEN_INGAME;
            controls->display_menu.row = 2;
        }
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
