#include "Menu.h"

#include "octaryn_client_render_distance.h"

#if defined(RUNTIME_CONTROLS_USE_SDL3)

namespace {

auto normalized_flag(uint8_t value) -> uint8_t
{
    return value != 0u ? 1u : 0u;
}

auto render_distance_option_index(int32_t render_distance) -> int32_t
{
    const int* options = octaryn_client_render_distance_options();
    const int count = octaryn_client_render_distance_option_count();
    const int sanitized = octaryn_client_render_distance_sanitize(render_distance);
    for (int index = 0; index < count; ++index)
    {
        if (options[index] == sanitized)
        {
            return index;
        }
    }
    return count > 0 ? 0 : -1;
}

auto window_is_fullscreen(SDL_Window* window) -> uint8_t
{
    if (window == nullptr)
    {
        return 0u;
    }

    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0u ? 1u : 0u;
}

auto mode_pixel_width(const SDL_DisplayMode* mode) -> int32_t
{
    if (mode == nullptr || mode->pixel_density <= 0.0f)
    {
        return 0;
    }
    return static_cast<int32_t>(static_cast<float>(mode->w) * mode->pixel_density + 0.5f);
}

auto mode_pixel_height(const SDL_DisplayMode* mode) -> int32_t
{
    if (mode == nullptr || mode->pixel_density <= 0.0f)
    {
        return 0;
    }
    return static_cast<int32_t>(static_cast<float>(mode->h) * mode->pixel_density + 0.5f);
}

auto selected_fullscreen_mode(
    SDL_DisplayID display,
    const display_catalog_mode& selected,
    SDL_DisplayMode* output) -> bool
{
    if (display == 0 || output == nullptr)
    {
        return false;
    }

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (modes == nullptr)
    {
        return false;
    }

    bool found = false;
    SDL_DisplayMode best{};
    for (int index = 0; index < count; ++index)
    {
        const SDL_DisplayMode* candidate = modes[index];
        if (candidate == nullptr ||
            mode_pixel_width(candidate) != selected.pixel_width ||
            mode_pixel_height(candidate) != selected.pixel_height)
        {
            continue;
        }
        if (!found || candidate->refresh_rate > best.refresh_rate)
        {
            best = *candidate;
            found = true;
        }
    }

    SDL_free(modes);
    if (found)
    {
        *output = best;
    }
    return found;
}

void center_window_on_display(SDL_Window* window, SDL_DisplayID display, int32_t width, int32_t height)
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

auto apply_display_menu(runtime_controls* controls, SDL_Window* window) -> uint32_t
{
    if (controls == nullptr || window == nullptr)
    {
        return 0u;
    }

    display_menu& menu = controls->display_menu;
    if (menu.apply_requested == 0u)
    {
        return 0u;
    }

    menu.apply_requested = 0u;
    if (menu.display_index >= 0 &&
        menu.display_index < controls->display_catalog.display_count &&
        menu.mode_index >= 0 &&
        menu.mode_index < controls->display_catalog.mode_count)
    {
        const SDL_DisplayID display = controls->display_catalog.displays[menu.display_index].id;
        const display_catalog_mode& mode =
            controls->display_catalog.modes[menu.mode_index];
        if (menu.display_dirty != 0u)
        {
            if (menu.fullscreen != 0u)
            {
                SDL_DisplayMode fullscreen_mode{};
                if (selected_fullscreen_mode(display, mode, &fullscreen_mode))
                {
                    SDL_SetWindowFullscreenMode(window, &fullscreen_mode);
                }
                SDL_SetWindowFullscreen(window, true);
            }
            else
            {
                SDL_SetWindowFullscreen(window, false);
                SDL_SetWindowSize(window, mode.width, mode.height);
                center_window_on_display(window, display, mode.width, mode.height);
            }
            SDL_SyncWindow(window);
        }
    }

    const int* options = octaryn_client_render_distance_options();
    const int option_count = octaryn_client_render_distance_option_count();
    if (menu.render_distance_index >= 0 && menu.render_distance_index < option_count)
    {
        controls->render_distance = options[menu.render_distance_index];
    }
    if (menu.present_mode_index >= 0 &&
        menu.present_mode_index < DISPLAY_MENU_PRESENT_MODE_COUNT)
    {
        controls->present_mode_index = menu.present_mode_index;
    }
    controls->fog_enabled = normalized_flag(menu.fog_enabled);
    controls->clouds_enabled = normalized_flag(menu.clouds_enabled);
    controls->sky_gradient_enabled = normalized_flag(menu.sky_gradient_enabled);
    controls->stars_enabled = normalized_flag(menu.stars_enabled);
    controls->sun_enabled = normalized_flag(menu.sun_enabled);
    controls->moon_enabled = normalized_flag(menu.moon_enabled);
    controls->pom_enabled = normalized_flag(menu.pom_enabled);
    controls->pbr_enabled = normalized_flag(menu.pbr_enabled);
    menu.display_dirty = 0u;
    runtime_controls_refresh_menu(controls, window, 0, 0);
    return RUNTIME_CONTROLS_MENU_APPLIED;
}

} // namespace

void runtime_controls_copy_to_menu(
    runtime_controls* controls,
    SDL_Window* window)
{
    if (controls == nullptr)
    {
        return;
    }

    display_menu& menu = controls->display_menu;
    menu.display_count = controls->display_catalog.display_count;
    menu.display_index = controls->display_catalog.display_index;
    menu.mode_count = controls->display_catalog.mode_count;
    menu.mode_index = controls->display_catalog.mode_index;
    menu.fullscreen = window_is_fullscreen(window);
    menu.present_mode_index = controls->present_mode_index;
    menu.fog_enabled = normalized_flag(controls->fog_enabled);
    menu.clouds_enabled = normalized_flag(controls->clouds_enabled);
    menu.sky_gradient_enabled = normalized_flag(controls->sky_gradient_enabled);
    menu.stars_enabled = normalized_flag(controls->stars_enabled);
    menu.sun_enabled = normalized_flag(controls->sun_enabled);
    menu.moon_enabled = normalized_flag(controls->moon_enabled);
    menu.pom_enabled = normalized_flag(controls->pom_enabled);
    menu.pbr_enabled = normalized_flag(controls->pbr_enabled);
    menu.render_distance_index = render_distance_option_index(controls->render_distance);
}

uint32_t runtime_controls_request_apply(
    runtime_controls* controls,
    SDL_Window* window)
{
    if (controls == nullptr)
    {
        return 0u;
    }

    display_menu_request_apply(&controls->display_menu);
    uint32_t result = RUNTIME_CONTROLS_MENU_CLOSED;
    result |= apply_display_menu(controls, window);
    runtime_controls_sync_relative_mouse(controls, window);
    return result;
}

uint32_t runtime_controls_close_menu(
    runtime_controls* controls,
    SDL_Window* window)
{
    return runtime_controls_request_apply(controls, window);
}

uint32_t runtime_controls_activate_menu_row(
    runtime_controls* controls,
    SDL_Window* window,
    int32_t row,
    int32_t delta)
{
    if (controls == nullptr ||
        row < 0 ||
        row >= DISPLAY_MENU_ROW_COUNT)
    {
        return RUNTIME_CONTROLS_EVENT_CAPTURED;
    }

    controls->display_menu.row = row;
    if (row == DISPLAY_MENU_APPLY_ROW)
    {
        return RUNTIME_CONTROLS_EVENT_CAPTURED |
            runtime_controls_request_apply(controls, window);
    }
    if (row == DISPLAY_MENU_CLOSE_ROW)
    {
        return RUNTIME_CONTROLS_EVENT_CAPTURED |
            runtime_controls_close_menu(controls, window);
    }
    if (row == DISPLAY_MENU_EXIT_ROW)
    {
        return RUNTIME_CONTROLS_EVENT_CAPTURED |
            runtime_controls_request_apply(controls, window) |
            RUNTIME_CONTROLS_QUIT_REQUESTED;
    }

    display_menu_adjust(
        &controls->display_menu,
        delta,
        octaryn_client_render_distance_option_count());
    return RUNTIME_CONTROLS_EVENT_CAPTURED;
}

int32_t runtime_controls_hit_menu_row(
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y)
{
    int width = viewport_width;
    int height = viewport_height;
    if ((width <= 0 || height <= 0) &&
        (window == nullptr || !SDL_GetWindowSizeInPixels(window, &width, &height)))
    {
        return -1;
    }

    int window_width = 0;
    int window_height = 0;
    if (window != nullptr &&
        SDL_GetWindowSize(window, &window_width, &window_height) &&
        window_width > 0 &&
        window_height > 0)
    {
        x *= static_cast<float>(width) / static_cast<float>(window_width);
        y *= static_cast<float>(height) / static_cast<float>(window_height);
    }

    return display_menu_hit_row(width, height, x, y);
}

#endif
