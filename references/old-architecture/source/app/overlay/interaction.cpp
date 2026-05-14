#include "app/overlay/interaction.h"

#include "app/lighting/settings/settings.h"
#include "app/runtime/internal.h"
#include "app/world_time/clock.h"

bool app_any_overlay_active(void)
{
    return display_menu.active || lighting_tuning.visible;
}

void app_sync_relative_mouse_mode_for_ui(void)
{
    if (app_any_overlay_active())
    {
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            restore_relative_mouse_after_ui = true;
            SDL_SetWindowRelativeMouseMode(window, false);
        }
        return;
    }

    if (restore_relative_mouse_after_ui)
    {
        SDL_SetWindowRelativeMouseMode(window, true);
        restore_relative_mouse_after_ui = false;
    }
}

void app_set_lighting_tuning_visible(bool visible)
{
    if (lighting_tuning.visible == visible)
    {
        return;
    }

    if (!visible && lighting_tuning_dirty)
    {
        main_lighting_settings_save(&lighting_tuning);
        lighting_tuning_dirty = false;
    }

    lighting_tuning.visible = visible;
    app_sync_relative_mouse_mode_for_ui();
}

bool app_handle_global_keydown(SDL_Scancode scancode)
{
    if (scancode == SDL_SCANCODE_F11)
    {
        main_window_toggle_fullscreen(window);
        app_sync_relative_mouse_mode_for_ui();
        return true;
    }
    if (scancode == SDL_SCANCODE_F3)
    {
        show_debug_overlay = !show_debug_overlay;
        return true;
    }
    if (scancode == SDL_SCANCODE_F6)
    {
        app_set_lighting_tuning_visible(!lighting_tuning.visible);
        return true;
    }
    if (scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS)
    {
        app_step_world_clock_hours(1);
        return true;
    }
    if (scancode == SDL_SCANCODE_MINUS || scancode == SDL_SCANCODE_KP_MINUS)
    {
        app_step_world_clock_hours(-1);
        return true;
    }
    return false;
}
