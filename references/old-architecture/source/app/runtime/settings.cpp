#include "settings.h"

#include "app/app_settings.h"
#include "app/startup_terrain/terrain.h"
#include "core/render_distance.h"
#include "runtime/jobs/runtime_worker_policy.h"
#include "world/runtime/world.h"

namespace {

constexpr Uint32 kAppSettingsVersion = 7;

auto worker_mode_is_auto() -> bool
{
    const char* manual_workers = SDL_getenv("OCTARYN_WORLD_MANUAL_WORKERS");
    if (manual_workers && manual_workers[0] != '\0')
    {
        return false;
    }
    return runtime_worker_mode_from_string(SDL_getenv("OCTARYN_WORLD_WORKER_MODE"), RUNTIME_WORKER_MODE_AUTO) ==
           RUNTIME_WORKER_MODE_AUTO;
}

auto sanitize_worker_count(int count) -> int
{
    const int worker_limit = world_get_generation_worker_limit();
    if (count <= 0)
    {
        return worker_mode_is_auto() ? 0 : worker_limit;
    }
    if (worker_mode_is_auto())
    {
        return 0;
    }
    return SDL_clamp(count, 1, worker_limit);
}

auto get_display_index(SDL_DisplayID display) -> int
{
    if (!display)
    {
        return 0;
    }
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    int result = 0;
    if (displays)
    {
        for (int i = 0; i < count; ++i)
        {
            if (displays[i] == display)
            {
                result = i;
                break;
            }
        }
        SDL_free(displays);
    }
    return result;
}

void copy_display_name(char* output, int output_size, SDL_DisplayID display)
{
    if (!output || output_size <= 0)
    {
        return;
    }
    const char* name = display ? SDL_GetDisplayName(display) : nullptr;
    SDL_snprintf(output, static_cast<size_t>(output_size), "%s", name ? name : "");
}

auto find_saved_display(const app_settings_t& app_settings) -> SDL_DisplayID
{
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    SDL_DisplayID fallback = SDL_GetPrimaryDisplay();
    if (!displays)
    {
        return fallback;
    }

    if (app_settings.display_index >= 0 && app_settings.display_index < count)
    {
        fallback = displays[app_settings.display_index];
    }
    if (app_settings.display_name[0] != '\0')
    {
        for (int i = 0; i < count; ++i)
        {
            const char* name = SDL_GetDisplayName(displays[i]);
            if (name && SDL_strcmp(name, app_settings.display_name) == 0)
            {
                fallback = displays[i];
                break;
            }
        }
    }

    SDL_free(displays);
    return fallback;
}

void center_window_on_display(SDL_Window* window, SDL_DisplayID display, int width, int height)
{
    if (!window || !display || width <= 0 || height <= 0)
    {
        return;
    }
    SDL_Rect bounds = {};
    if (SDL_GetDisplayBounds(display, &bounds))
    {
        SDL_SetWindowPosition(window, bounds.x + (bounds.w - width) / 2, bounds.y + (bounds.h - height) / 2);
    }
}

void capture_display_settings(const main_runtime_settings_t* settings, app_settings_t* app_settings)
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(settings->window);
    if (!display)
    {
        display = SDL_GetPrimaryDisplay();
    }
    copy_display_name(app_settings->display_name, OCTARYN_SETTINGS_DISPLAY_NAME_CAPACITY, display);
    app_settings->display_index = get_display_index(display);

    const SDL_DisplayMode* fullscreen_mode = SDL_GetWindowFullscreenMode(settings->window);
    if (fullscreen_mode)
    {
        app_settings->display_mode_width = fullscreen_mode->w;
        app_settings->display_mode_height = fullscreen_mode->h;
        app_settings->display_mode_refresh_rate = fullscreen_mode->refresh_rate;
    }
    else
    {
        app_settings->display_mode_width = app_settings->window_width;
        app_settings->display_mode_height = app_settings->window_height;
        app_settings->display_mode_refresh_rate = 0.0f;
    }
}

void restore_window_display_and_mode(SDL_Window* window, const app_settings_t& app_settings)
{
    const SDL_DisplayID display = find_saved_display(app_settings);
    const int width = app_settings.display_mode_width > 0 ? app_settings.display_mode_width : app_settings.window_width;
    const int height = app_settings.display_mode_height > 0 ? app_settings.display_mode_height : app_settings.window_height;

    if (width > 0 && height > 0)
    {
        SDL_SetWindowSize(window, width, height);
        center_window_on_display(window, display, width, height);
    }

    if (!app_settings.fullscreen)
    {
        return;
    }

    SDL_DisplayMode mode = {};
    const bool has_exact_mode = width > 0 && height > 0 &&
                                SDL_GetClosestFullscreenDisplayMode(display,
                                                                    width,
                                                                    height,
                                                                    app_settings.display_mode_refresh_rate,
                                                                    true,
                                                                    &mode);
    if (has_exact_mode && !SDL_SetWindowFullscreenMode(window, &mode))
    {
        SDL_Log("Failed to restore fullscreen mode: %s", SDL_GetError());
    }
    if (!SDL_SetWindowFullscreen(window, true))
    {
        SDL_Log("Failed to restore fullscreen: %s", SDL_GetError());
    }
    SDL_SetWindowRelativeMouseMode(window, false);
}

} // namespace

void main_runtime_settings_init(main_runtime_settings_t* settings, SDL_Window* window, bool* fog_enabled,
                                bool* clouds_enabled, bool* sky_gradient_enabled, bool* stars_enabled,
                                bool* sun_enabled, bool* moon_enabled, bool* pom_enabled, bool* pbr_enabled,
                                int* startup_render_distance, int* worldgen_worker_count)
{
    *settings = {};
    settings->window = window;
    settings->fog_enabled = fog_enabled;
    settings->clouds_enabled = clouds_enabled;
    settings->sky_gradient_enabled = sky_gradient_enabled;
    settings->stars_enabled = stars_enabled;
    settings->sun_enabled = sun_enabled;
    settings->moon_enabled = moon_enabled;
    settings->pom_enabled = pom_enabled;
    settings->pbr_enabled = pbr_enabled;
    settings->startup_render_distance = startup_render_distance;
    settings->worldgen_worker_count = worldgen_worker_count;
    *settings->worldgen_worker_count = sanitize_worker_count(*settings->worldgen_worker_count);
}

void main_runtime_settings_refresh_worker_options(int* options, int* option_count, int capacity)
{
    if (options == nullptr || option_count == nullptr || capacity <= 0)
    {
        return;
    }

    options[0] = 0;
    *option_count = 1;
}

static app_settings_t capture_app_settings(const main_runtime_settings_t* settings)
{
    app_settings_t app_settings = {};
    app_settings.version = kAppSettingsVersion;
    app_settings.fog_enabled = *settings->fog_enabled;
    app_settings.fullscreen = (SDL_GetWindowFlags(settings->window) & SDL_WINDOW_FULLSCREEN) != 0;
    app_settings.clouds_enabled = *settings->clouds_enabled;
    app_settings.sky_gradient_enabled = *settings->sky_gradient_enabled;
    app_settings.stars_enabled = *settings->stars_enabled;
    app_settings.sun_enabled = *settings->sun_enabled;
    app_settings.moon_enabled = *settings->moon_enabled;
    app_settings.pom_enabled = *settings->pom_enabled;
    app_settings.pbr_enabled = *settings->pbr_enabled;
    SDL_GetWindowSize(settings->window, &app_settings.window_width, &app_settings.window_height);
    capture_display_settings(settings, &app_settings);
    app_settings.render_distance = octaryn_sanitize_render_distance(app_render_distance_setting(world_get_render_distance()));
    app_settings.worldgen_threads = sanitize_worker_count(world_get_generation_worker_count());
    return app_settings;
}

void main_runtime_settings_persist(const main_runtime_settings_t* settings)
{
    const app_settings_t app_settings = capture_app_settings(settings);
    app_settings_save(&app_settings);
}

void main_runtime_settings_load(main_runtime_settings_t* settings)
{
    app_settings_t app_settings = {};
    if (!app_settings_load(&app_settings))
    {
        return;
    }
    *settings->fog_enabled = app_settings.fog_enabled;
    *settings->clouds_enabled = app_settings.clouds_enabled != 0;
    *settings->sky_gradient_enabled = app_settings.sky_gradient_enabled != 0;
    *settings->stars_enabled = app_settings.stars_enabled != 0;
    *settings->sun_enabled = app_settings.sun_enabled != 0;
    *settings->moon_enabled = app_settings.moon_enabled != 0;
    *settings->pom_enabled = app_settings.pom_enabled != 0;
    *settings->pbr_enabled = app_settings.pbr_enabled != 0;
    *settings->startup_render_distance = octaryn_sanitize_render_distance(app_settings.render_distance);
    *settings->worldgen_worker_count = sanitize_worker_count(app_settings.worldgen_threads);
    restore_window_display_and_mode(settings->window, app_settings);
}
