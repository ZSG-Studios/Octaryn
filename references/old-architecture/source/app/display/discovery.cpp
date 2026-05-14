#include "internal.h"

#include "app/startup_terrain/terrain.h"

#include "world/runtime/world.h"

namespace app_display_internal {

auto compare_display_modes(const SDL_DisplayMode& left, const SDL_DisplayMode& right) -> int
{
    const int left_width = main_display_menu_mode_pixel_width(&left);
    const int left_height = main_display_menu_mode_pixel_height(&left);
    const int right_width = main_display_menu_mode_pixel_width(&right);
    const int right_height = main_display_menu_mode_pixel_height(&right);
    const Uint64 left_area = static_cast<Uint64>(left_width) * static_cast<Uint64>(left_height);
    const Uint64 right_area = static_cast<Uint64>(right_width) * static_cast<Uint64>(right_height);
    if (left_area != right_area)
    {
        return left_area < right_area ? -1 : 1;
    }
    if (left_width != right_width)
    {
        return left_width < right_width ? -1 : 1;
    }
    if (left_height != right_height)
    {
        return left_height < right_height ? -1 : 1;
    }
    if (left.refresh_rate != right.refresh_rate)
    {
        return left.refresh_rate < right.refresh_rate ? -1 : 1;
    }
    return 0;
}

void sort_display_modes(main_display_menu_t* menu)
{
    for (int i = 1; i < menu->mode_count; ++i)
    {
        SDL_DisplayMode mode = menu->modes[i];
        int j = i - 1;
        while (j >= 0 && compare_display_modes(mode, menu->modes[j]) < 0)
        {
            menu->modes[j + 1] = menu->modes[j];
            --j;
        }
        menu->modes[j + 1] = mode;
    }
}

auto get_distance_option_index(const main_display_menu_runtime_t* runtime, int chunks) -> int
{
    for (int i = 0; i < runtime->distance_option_count; i++)
    {
        if (runtime->distance_options[i] == chunks)
        {
            return i;
        }
    }
    return 0;
}

auto get_window_display_index(const main_display_menu_t* menu, SDL_Window* window) -> int
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display)
    {
        display = SDL_GetPrimaryDisplay();
    }
    for (int i = 0; i < menu->display_count; i++)
    {
        if (menu->displays[i] == display)
        {
            return i;
        }
    }
    return 0;
}

void refresh_display_modes(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime)
{
    menu->mode_count = 0;
    if (menu->display_index < 0 || menu->display_index >= menu->display_count)
    {
        return;
    }
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(menu->displays[menu->display_index], &count);
    if (!modes)
    {
        return;
    }
    for (int i = 0; i < count && menu->mode_count < MAIN_DISPLAY_MENU_MAX_MODES; i++)
    {
        SDL_DisplayMode* candidate = modes[i];
        int pixel_width = main_display_menu_mode_pixel_width(candidate);
        int pixel_height = main_display_menu_mode_pixel_height(candidate);
        int existing = -1;
        for (int j = 0; j < menu->mode_count; j++)
        {
            if (main_display_menu_mode_pixel_width(&menu->modes[j]) == pixel_width &&
                main_display_menu_mode_pixel_height(&menu->modes[j]) == pixel_height)
            {
                existing = j;
                break;
            }
        }
        if (existing >= 0)
        {
            if (candidate->refresh_rate > menu->modes[existing].refresh_rate)
            {
                menu->modes[existing] = *candidate;
            }
        }
        else
        {
            menu->modes[menu->mode_count++] = *candidate;
        }
    }
    SDL_free(modes);
    sort_display_modes(menu);
    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSizeInPixels(runtime->window, &window_width, &window_height))
    {
        window_width = runtime->player->camera.viewport_size[0];
        window_height = runtime->player->camera.viewport_size[1];
    }
    menu->mode_index = 0;
    for (int i = 0; i < menu->mode_count; i++)
    {
        if (main_display_menu_mode_pixel_width(&menu->modes[i]) == window_width &&
            main_display_menu_mode_pixel_height(&menu->modes[i]) == window_height)
        {
            menu->mode_index = i;
            break;
        }
    }
}

void refresh_display_menu(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime)
{
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    menu->display_count = 0;
    if (displays)
    {
        for (int i = 0; i < count && i < MAIN_DISPLAY_MENU_MAX_DISPLAYS; i++)
        {
            menu->displays[menu->display_count++] = displays[i];
        }
        SDL_free(displays);
    }
    if (menu->display_count <= 0)
    {
        menu->display_count = 1;
        menu->displays[0] = SDL_GetPrimaryDisplay();
    }
    menu->display_index = get_window_display_index(menu, runtime->window);
    menu->fullscreen = (SDL_GetWindowFlags(runtime->window) & SDL_WINDOW_FULLSCREEN) != 0;
    menu->fog_enabled = *runtime->fog_enabled;
    menu->clouds_enabled = *runtime->clouds_enabled;
    menu->sky_gradient_enabled = *runtime->sky_gradient_enabled;
    menu->stars_enabled = *runtime->stars_enabled;
    menu->sun_enabled = *runtime->sun_enabled;
    menu->moon_enabled = *runtime->moon_enabled;
    menu->pom_enabled = *runtime->pom_enabled;
    menu->pbr_enabled = *runtime->pbr_enabled;
    menu->render_distance_index =
        get_distance_option_index(runtime, app_render_distance_setting(world_get_render_distance()));
    menu->display_dirty = false;
    refresh_display_modes(menu, runtime);
}

} // namespace app_display_internal
