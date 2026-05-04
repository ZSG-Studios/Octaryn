#include "octaryn_client_runtime_controls.h"

#include "octaryn_client_render_distance.h"
#include "octaryn_client_window_lifecycle.h"

#if defined(OCTARYN_CLIENT_RUNTIME_CONTROLS_USE_SDL3)

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

void copy_runtime_to_menu(octaryn_client_runtime_controls* controls, SDL_Window* window)
{
    if (controls == nullptr)
    {
        return;
    }

    octaryn_client_display_menu& menu = controls->display_menu;
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
    const octaryn_client_display_catalog_mode& selected,
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

auto apply_display_menu(octaryn_client_runtime_controls* controls, SDL_Window* window) -> uint32_t
{
    if (controls == nullptr || window == nullptr)
    {
        return 0u;
    }

    octaryn_client_display_menu& menu = controls->display_menu;
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
        const octaryn_client_display_catalog_mode& mode =
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
        menu.present_mode_index < OCTARYN_CLIENT_DISPLAY_MENU_PRESENT_MODE_COUNT)
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
    octaryn_client_runtime_controls_refresh_menu(controls, window, 0, 0);
    return OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_APPLIED;
}

auto request_apply(octaryn_client_runtime_controls* controls, SDL_Window* window) -> uint32_t
{
    if (controls == nullptr)
    {
        return 0u;
    }

    octaryn_client_display_menu_request_apply(&controls->display_menu);
    uint32_t result = OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_CLOSED;
    result |= apply_display_menu(controls, window);
    octaryn_client_runtime_controls_sync_relative_mouse(controls, window);
    return result;
}

auto close_menu(octaryn_client_runtime_controls* controls, SDL_Window* window) -> uint32_t
{
    return request_apply(controls, window);
}

auto activate_menu_row(octaryn_client_runtime_controls* controls, SDL_Window* window, int32_t row, int32_t delta)
    -> uint32_t
{
    if (controls == nullptr ||
        row < 0 ||
        row >= OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT)
    {
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
    }

    controls->display_menu.row = row;
    if (row == OCTARYN_CLIENT_DISPLAY_MENU_APPLY_ROW)
    {
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED | request_apply(controls, window);
    }
    if (row == OCTARYN_CLIENT_DISPLAY_MENU_CLOSE_ROW)
    {
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED | close_menu(controls, window);
    }
    if (row == OCTARYN_CLIENT_DISPLAY_MENU_EXIT_ROW)
    {
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED |
            request_apply(controls, window) |
            OCTARYN_CLIENT_RUNTIME_CONTROLS_QUIT_REQUESTED;
    }

    octaryn_client_display_menu_adjust(
        &controls->display_menu,
        delta,
        octaryn_client_render_distance_option_count());
    return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
}

auto hit_menu_row(SDL_Window* window, int32_t viewport_width, int32_t viewport_height, float x, float y) -> int32_t
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

    return octaryn_client_display_menu_hit_row(width, height, x, y);
}

} // namespace

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

    octaryn_client_display_catalog_refresh(
        &controls->display_catalog,
        window,
        viewport_width,
        viewport_height);
    copy_runtime_to_menu(controls, window);
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

uint32_t octaryn_client_runtime_controls_handle_event(
    octaryn_client_runtime_controls* controls,
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
            octaryn_client_window_lifecycle_toggle_fullscreen(window);
            octaryn_client_runtime_controls_refresh_menu(controls, window, viewport_width, viewport_height);
            octaryn_client_runtime_controls_sync_relative_mouse(controls, window);
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED |
                OCTARYN_CLIENT_RUNTIME_CONTROLS_FULLSCREEN_TOGGLED;
        }
        if (event->key.scancode == SDL_SCANCODE_F3)
        {
            controls->debug_overlay_enabled = controls->debug_overlay_enabled == 0u ? 1u : 0u;
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED |
                OCTARYN_CLIENT_RUNTIME_CONTROLS_DEBUG_TOGGLED;
        }
    }

    if (octaryn_client_runtime_controls_ui_active(controls) != 0u)
    {
        if (event->type == SDL_EVENT_MOUSE_MOTION)
        {
            const int32_t row = hit_menu_row(window, viewport_width, viewport_height, event->motion.x, event->motion.y);
            if (row >= 0)
            {
                controls->display_menu.row = row;
            }
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
            {
                const int32_t row = hit_menu_row(window, viewport_width, viewport_height, event->button.x, event->button.y);
                return activate_menu_row(
                    controls,
                    window,
                    row,
                    event->button.button == SDL_BUTTON_LEFT ? 1 : -1);
            }
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_MOUSE_WHEEL)
        {
            if (event->wheel.y > 0.0f)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT - 1) %
                    OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->wheel.y < 0.0f)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + 1) % OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT;
            }
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
        }
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
        {
            if (event->key.scancode == SDL_SCANCODE_ESCAPE)
            {
                return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED | close_menu(controls, window);
            }
            if (event->key.scancode == SDL_SCANCODE_UP)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT - 1) %
                    OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_DOWN)
            {
                controls->display_menu.row =
                    (controls->display_menu.row + 1) % OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT;
            }
            else if (event->key.scancode == SDL_SCANCODE_LEFT)
            {
                octaryn_client_display_menu_adjust(
                    &controls->display_menu,
                    -1,
                    octaryn_client_render_distance_option_count());
            }
            else if (event->key.scancode == SDL_SCANCODE_RIGHT)
            {
                octaryn_client_display_menu_adjust(
                    &controls->display_menu,
                    1,
                    octaryn_client_render_distance_option_count());
            }
            else if (event->key.scancode == SDL_SCANCODE_RETURN ||
                     event->key.scancode == SDL_SCANCODE_KP_ENTER)
            {
                return activate_menu_row(controls, window, controls->display_menu.row, 1);
            }
            return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
        }

        return 0u;
    }

    if (event->type == SDL_EVENT_KEY_DOWN &&
        !event->key.repeat &&
        event->key.scancode == SDL_SCANCODE_ESCAPE)
    {
        octaryn_client_runtime_controls_refresh_menu(controls, window, viewport_width, viewport_height);
        octaryn_client_display_menu_open(&controls->display_menu);
        octaryn_client_runtime_controls_sync_relative_mouse(controls, window);
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED |
            OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_OPENED;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        !SDL_GetWindowRelativeMouseMode(window))
    {
        SDL_SetWindowRelativeMouseMode(window, true);
        controls->restore_relative_mouse_after_ui = 0u;
        return OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED;
    }

    return 0u;
}

#else

void octaryn_client_runtime_controls_init(octaryn_client_runtime_controls* controls)
{
    if (controls != nullptr)
    {
        *controls = {};
    }
}

uint8_t octaryn_client_runtime_controls_ui_active(const octaryn_client_runtime_controls* controls)
{
    (void)controls;
    return 0u;
}

void octaryn_client_runtime_controls_refresh_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height)
{
    (void)controls;
    (void)window;
    (void)viewport_width;
    (void)viewport_height;
}

void octaryn_client_runtime_controls_sync_relative_mouse(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window)
{
    (void)controls;
    (void)window;
}

uint32_t octaryn_client_runtime_controls_handle_event(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event,
    int32_t viewport_width,
    int32_t viewport_height)
{
    (void)controls;
    (void)window;
    (void)event;
    (void)viewport_width;
    (void)viewport_height;
    return 0u;
}

#endif
