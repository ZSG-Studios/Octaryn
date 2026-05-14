#include "internal.h"

namespace {

constexpr float kUiReferenceWidth = 1280.0f;
constexpr float kUiReferenceHeight = 720.0f;
constexpr float kUiScale = 2.0f;
constexpr int kTextColumns = 24;

int fit_panel_font_scale(int requested_font_scale, int rows, int viewport_height)
{
    const int available_height = SDL_max(1, viewport_height - 16);
    const int needed_units = rows * 6 + 8;
    const int fitted = SDL_max(1, available_height / needed_units);
    return SDL_max(1, SDL_min(requested_font_scale, fitted));
}

int wrap_menu_index(int index, int delta, int count)
{
    if (count <= 0)
    {
        return 0;
    }
    return (index + delta + count) % count;
}

} // namespace

int main_display_menu_mode_pixel_width(const SDL_DisplayMode* mode)
{
    return static_cast<int>(static_cast<float>(mode->w) * mode->pixel_density + 0.5f);
}

int main_display_menu_mode_pixel_height(const SDL_DisplayMode* mode)
{
    return static_cast<int>(static_cast<float>(mode->h) * mode->pixel_density + 0.5f);
}

void main_display_menu_open(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime)
{
    menu->active = true;
    menu->row = 0;
    SDL_SetWindowRelativeMouseMode(runtime->window, false);
    app_display_internal::refresh_display_menu(menu, runtime);
}

void main_display_menu_close(main_display_menu_t* menu)
{
    menu->active = false;
}

void main_display_menu_adjust(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime, int delta)
{
    if (menu->row == 0)
    {
        menu->display_index = wrap_menu_index(menu->display_index, delta, menu->display_count);
        menu->display_dirty = true;
        app_display_internal::refresh_display_modes(menu, runtime);
    }
    else if (menu->row == 1 && menu->mode_count > 0)
    {
        menu->mode_index = wrap_menu_index(menu->mode_index, delta, menu->mode_count);
        menu->display_dirty = true;
    }
    else if (menu->row == 2)
    {
        menu->fullscreen = !menu->fullscreen;
        menu->display_dirty = true;
    }
    else if (menu->row == 3)
    {
        menu->render_distance_index = wrap_menu_index(menu->render_distance_index, delta, runtime->distance_option_count);
    }
    else if (menu->row == 4)
    {
        menu->fog_enabled = !menu->fog_enabled;
    }
    else if (menu->row == 5)
    {
        menu->clouds_enabled = !menu->clouds_enabled;
    }
    else if (menu->row == 6)
    {
        menu->sky_gradient_enabled = !menu->sky_gradient_enabled;
    }
    else if (menu->row == 7)
    {
        menu->stars_enabled = !menu->stars_enabled;
    }
    else if (menu->row == 8)
    {
        menu->sun_enabled = !menu->sun_enabled;
    }
    else if (menu->row == 9)
    {
        menu->moon_enabled = !menu->moon_enabled;
    }
    else if (menu->row == 10)
    {
        menu->pom_enabled = !menu->pom_enabled;
    }
    else if (menu->row == 11)
    {
        menu->pbr_enabled = !menu->pbr_enabled;
    }
}

int main_display_menu_hit_row(int viewport_width, int viewport_height, float x, float y)
{
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        return -1;
    }

    const float base_scale = SDL_max(static_cast<float>(viewport_width) / kUiReferenceWidth,
                                     static_cast<float>(viewport_height) / kUiReferenceHeight);
    const float scale = base_scale * kUiScale;
    const int font_scale = fit_panel_font_scale(SDL_max(1, static_cast<int>(scale + 0.5f)),
                                                MAIN_DISPLAY_MENU_ROW_COUNT,
                                                viewport_height);
    const int padding = 4 * font_scale;
    const int content_width = kTextColumns * 4 * font_scale - font_scale;
    const int content_height = MAIN_DISPLAY_MENU_ROW_COUNT * 6 * font_scale - font_scale;
    const int panel_width = content_width + padding * 2;
    const int panel_height = content_height + padding * 2;
    const int panel_min_x = (viewport_width - panel_width) / 2;
    const int panel_min_y = (viewport_height - panel_height) / 2;
    const int text_origin_x = panel_min_x + padding;
    const int text_origin_y = panel_min_y + padding;
    const int line_advance = 6 * font_scale;

    const int mouse_x = static_cast<int>(x);
    const int mouse_y = viewport_height - 1 - static_cast<int>(y);
    if (mouse_x < panel_min_x || mouse_x >= panel_min_x + panel_width ||
        mouse_y < panel_min_y || mouse_y >= panel_min_y + panel_height)
    {
        return -1;
    }
    if (mouse_x < text_origin_x || mouse_x >= text_origin_x + content_width ||
        mouse_y < text_origin_y - font_scale || mouse_y >= text_origin_y + content_height + font_scale)
    {
        return -1;
    }

    const int row_from_bottom = (mouse_y - text_origin_y + font_scale) / line_advance;
    const int row = MAIN_DISPLAY_MENU_ROW_COUNT - 1 - row_from_bottom;
    if (row < 0 || row >= MAIN_DISPLAY_MENU_ROW_COUNT)
    {
        return -1;
    }
    return row;
}
