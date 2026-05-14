#include "internal.h"

#include "app/startup_terrain/terrain.h"
#include "world/runtime/world.h"

namespace app_display_internal {

void center_window_on_display(SDL_Window* window, SDL_DisplayID display, int width, int height)
{
    SDL_Rect bounds = {};
    if (!SDL_GetDisplayBounds(display, &bounds))
    {
        return;
    }
    SDL_SetWindowPosition(window, bounds.x + (bounds.w - width) / 2, bounds.y + (bounds.h - height) / 2);
}

void commit_display_menu(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime)
{
    if (menu->display_index < 0 || menu->display_index >= menu->display_count ||
        menu->mode_index < 0 || menu->mode_index >= menu->mode_count)
    {
        return;
    }
    SDL_DisplayID display = menu->displays[menu->display_index];
    SDL_DisplayMode mode = menu->modes[menu->mode_index];
    if (menu->display_dirty)
    {
        center_window_on_display(runtime->window, display, mode.w, mode.h);
        if (menu->fullscreen)
        {
            if (!SDL_SetWindowFullscreenMode(runtime->window, &mode))
            {
                SDL_Log("Failed to set fullscreen mode: %s", SDL_GetError());
                return;
            }
            if (!SDL_SetWindowFullscreen(runtime->window, true))
            {
                SDL_Log("Failed to enter fullscreen: %s", SDL_GetError());
                return;
            }
        }
        else
        {
            if (!SDL_SetWindowFullscreen(runtime->window, false))
            {
                SDL_Log("Failed to leave fullscreen: %s", SDL_GetError());
                return;
            }
            if (!SDL_SetWindowSize(runtime->window, mode.w, mode.h))
            {
                SDL_Log("Failed to set window size: %s", SDL_GetError());
                return;
            }
        }
        if (!SDL_SyncWindow(runtime->window))
        {
            SDL_Log("Failed to sync window state: %s", SDL_GetError());
        }
    }
    *runtime->fog_enabled = menu->fog_enabled;
    *runtime->clouds_enabled = menu->clouds_enabled;
    *runtime->sky_gradient_enabled = menu->sky_gradient_enabled;
    *runtime->stars_enabled = menu->stars_enabled;
    *runtime->sun_enabled = menu->sun_enabled;
    *runtime->moon_enabled = menu->moon_enabled;
    *runtime->pom_enabled = menu->pom_enabled;
    *runtime->pbr_enabled = menu->pbr_enabled;
    int next_render_distance = runtime->distance_options[menu->render_distance_index];
    app_set_render_distance_setting(next_render_distance, world_get_render_distance());
    if (next_render_distance != world_get_render_distance())
    {
        world_set_render_distance(&runtime->player->camera, next_render_distance);
    }
    refresh_display_menu(menu, runtime);
    if (runtime->persist_settings)
    {
        runtime->persist_settings();
    }
}

} // namespace app_display_internal
