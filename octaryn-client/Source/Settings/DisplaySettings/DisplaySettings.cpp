#include "DisplaySettings.h"

#if defined(OCTARYN_CLIENT_DISPLAY_SETTINGS_USE_SDL3)

#include <cstdio>
#include <cmath>

namespace {

auto primary_display_or_zero() -> SDL_DisplayID
{
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    return display != 0 ? display : 0;
}

void copy_display_name(char* output, int output_size, SDL_DisplayID display)
{
    if (output == nullptr || output_size <= 0)
    {
        return;
    }

    output[0] = '\0';
    const char* name = display != 0 ? SDL_GetDisplayName(display) : nullptr;
    if (name != nullptr)
    {
        std::snprintf(output, static_cast<size_t>(output_size), "%s", name);
    }
}

void center_window_on_display(SDL_Window* window, SDL_DisplayID display, int width, int height)
{
    if (window == nullptr || display == 0 || width <= 0 || height <= 0)
    {
        return;
    }

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(display, &bounds))
    {
        SDL_SetWindowPosition(
            window,
            bounds.x + (bounds.w - width) / 2,
            bounds.y + (bounds.h - height) / 2);
    }
}

} // namespace

int display_settings_display_index(SDL_DisplayID display)
{
    if (display == 0)
    {
        return -1;
    }

    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (displays == nullptr)
    {
        return -1;
    }

    int index = -1;
    for (int candidate_index = 0; candidate_index < count; ++candidate_index)
    {
        if (displays[candidate_index] == display)
        {
            index = candidate_index;
            break;
        }
    }
    SDL_free(displays);
    return index;
}

void display_settings_capture(
    app_settings* settings,
    SDL_Window* window)
{
    if (settings == nullptr || window == nullptr)
    {
        return;
    }

    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0)
    {
        display = primary_display_or_zero();
    }

    copy_display_name(
        settings->display_name,
        static_cast<int>(APP_SETTINGS_DISPLAY_NAME_CAPACITY),
        display);
    settings->display_index = display_settings_display_index(display);

    const SDL_DisplayMode* fullscreen_mode = SDL_GetWindowFullscreenMode(window);
    if (fullscreen_mode != nullptr)
    {
        settings->display_mode_width = fullscreen_mode->w;
        settings->display_mode_height = fullscreen_mode->h;
        settings->display_mode_refresh_rate = fullscreen_mode->refresh_rate;
    }
    else
    {
        settings->display_mode_width = settings->window_width;
        settings->display_mode_height = settings->window_height;
        settings->display_mode_refresh_rate = 0.0f;
    }
}

SDL_DisplayID display_settings_resolve_display(
    const app_settings* settings)
{
    if (settings == nullptr)
    {
        return primary_display_or_zero();
    }

    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (displays == nullptr)
    {
        return primary_display_or_zero();
    }

    SDL_DisplayID display = 0;
    if (settings->display_index >= 0 && settings->display_index < count)
    {
        display = displays[settings->display_index];
    }

    if (settings->display_name[0] != '\0')
    {
        for (int index = 0; index < count; ++index)
        {
            const char* name = SDL_GetDisplayName(displays[index]);
            if (name != nullptr && SDL_strcmp(name, settings->display_name) == 0)
            {
                display = displays[index];
                break;
            }
        }
    }

    SDL_free(displays);
    return display != 0 ? display : primary_display_or_zero();
}

int display_settings_restore_window(
    SDL_Window* window,
    const app_settings* settings)
{
    if (window == nullptr || settings == nullptr)
    {
        return 0;
    }

    const SDL_DisplayID display = display_settings_resolve_display(settings);
    const int width = settings->window_width > 0 ? settings->window_width : 1280;
    const int height = settings->window_height > 0 ? settings->window_height : 720;
    center_window_on_display(window, display, width, height);
    if (settings->fullscreen != 0u) {
        int count{};
        SDL_DisplayMode** modes=SDL_GetFullscreenDisplayModes(display,&count);
        SDL_DisplayMode selected{};
        bool found=false;
        for(int i=0;modes && i<count;++i) {
            const auto* mode=modes[i];
            if(!mode || mode->w!=settings->display_mode_width || mode->h!=settings->display_mode_height) continue;
            if(!found || (settings->display_mode_refresh_rate>0
                ? std::abs(mode->refresh_rate-settings->display_mode_refresh_rate)<std::abs(selected.refresh_rate-settings->display_mode_refresh_rate)
                : mode->refresh_rate>selected.refresh_rate)) { selected=*mode;found=true; }
        }
        SDL_free(modes);
        if(found && !SDL_SetWindowFullscreenMode(window,&selected)) return 0;
        if(!SDL_SetWindowFullscreen(window,true)) return 0;
    } else {
        if(!SDL_SetWindowFullscreen(window,false) || !SDL_SetWindowSize(window,width,height)) return 0;
    }
    SDL_SyncWindow(window);
    return display != 0;
}

#else

int display_settings_display_index(SDL_DisplayID display)
{
    (void)display;
    return -1;
}

void display_settings_capture(
    app_settings* settings,
    SDL_Window* window)
{
    (void)settings;
    (void)window;
}

SDL_DisplayID display_settings_resolve_display(
    const app_settings* settings)
{
    (void)settings;
    return 0;
}

int display_settings_restore_window(
    SDL_Window* window,
    const app_settings* settings)
{
    (void)window;
    (void)settings;
    return 0;
}

#endif
