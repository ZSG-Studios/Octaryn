#include "DisplayMenu.h"

#include <cstring>

namespace {

constexpr float kUiReferenceWidth = 1280.0f;
constexpr float kUiReferenceHeight = 720.0f;
constexpr float kUiScale = 2.0f;
constexpr int32_t kTextColumns = 30;

auto max_int(int32_t left, int32_t right) -> int32_t
{
    return left > right ? left : right;
}

auto min_int(int32_t left, int32_t right) -> int32_t
{
    return left < right ? left : right;
}

auto max_float(float left, float right) -> float
{
    return left > right ? left : right;
}

auto fit_panel_font_scale(int32_t requested_font_scale, int32_t rows, int32_t viewport_height) -> int32_t
{
    const int32_t available_height = max_int(1, viewport_height - 16);
    const int32_t needed_units = rows * 6 + 8;
    const int32_t fitted = max_int(1, available_height / needed_units);
    return max_int(1, min_int(requested_font_scale, fitted));
}

auto wrap_menu_index(int32_t index, int32_t delta, int32_t count) -> int32_t
{
    if (count <= 0)
    {
        return 0;
    }

    return (index + delta + count) % count;
}

auto normalized_flag(uint8_t value) -> uint8_t
{
    return value != 0u ? 1u : 0u;
}

auto toggled_flag(uint8_t value) -> uint8_t
{
    return normalized_flag(value) == 0u ? 1u : 0u;
}

auto row_is_one_of(int32_t row, int32_t a, int32_t b, int32_t c, int32_t d) -> bool
{
    return row == a || row == b || row == c || row == d;
}

} // namespace

int32_t display_menu_mode_pixel_width(int32_t mode_width, float pixel_density)
{
    if (mode_width <= 0 || pixel_density <= 0.0f)
    {
        return 0;
    }

    return static_cast<int32_t>(static_cast<float>(mode_width) * pixel_density + 0.5f);
}

int32_t display_menu_mode_pixel_height(int32_t mode_height, float pixel_density)
{
    if (mode_height <= 0 || pixel_density <= 0.0f)
    {
        return 0;
    }

    return static_cast<int32_t>(static_cast<float>(mode_height) * pixel_density + 0.5f);
}

void display_menu_open(display_menu* menu)
{
    if (menu == nullptr)
    {
        return;
    }

    menu->active = 1u;
    menu->screen = DISPLAY_MENU_SCREEN_MAIN;
    menu->action_requested = DISPLAY_MENU_ACTION_NONE;
    menu->world_slot = 0u;
    menu->editing_field = 0u;
    menu->status_code = 0u;
    std::strncpy(menu->world_name, "WORLD 1",
                 DISPLAY_MENU_WORLD_NAME_SIZE - 1);
    std::strncpy(menu->server_address, "127.0.0.1",
                 DISPLAY_MENU_SERVER_ADDRESS_SIZE - 1);
    std::strncpy(menu->server_port, "7777", DISPLAY_MENU_SERVER_PORT_SIZE - 1);
    menu->row = 2;
}

void display_menu_close(display_menu* menu)
{
    if (menu == nullptr)
    {
        return;
    }

    menu->active = 0u;
}

uint8_t display_menu_row_selectable(const display_menu* menu, int32_t row)
{
    if (menu == nullptr || row < 0 || row >= DISPLAY_MENU_ROW_COUNT)
    {
        return 0u;
    }

    if (menu->screen == DISPLAY_MENU_SCREEN_MAIN)
    {
        return row_is_one_of(row, 2, 3, 4, DISPLAY_MENU_EXIT_ROW) ? 1u : 0u;
    }
    if (menu->screen == DISPLAY_MENU_SCREEN_INGAME)
    {
        return (row_is_one_of(row, 2, 3, 4, DISPLAY_MENU_EXIT_ROW) || row == 5) ? 1u : 0u;
    }
    if (menu->screen == DISPLAY_MENU_SCREEN_SINGLEPLAYER)
    {
        return (row >= 2 && row <= 9) || row == DISPLAY_MENU_CLOSE_ROW ? 1u : 0u;
    }
    if (menu->screen == DISPLAY_MENU_SCREEN_MULTIPLAYER)
    {
        return (row >= 2 && row <= 5) || row == DISPLAY_MENU_CLOSE_ROW ? 1u : 0u;
    }
    return 1u;
}

void display_menu_move_row(display_menu* menu, int32_t delta)
{
    if (menu == nullptr || delta == 0)
    {
        return;
    }

    int32_t row = menu->row;
    for (int32_t step = 0; step < DISPLAY_MENU_ROW_COUNT; ++step)
    {
        row = wrap_menu_index(row, delta > 0 ? 1 : -1, DISPLAY_MENU_ROW_COUNT);
        if (display_menu_row_selectable(menu, row) != 0u)
        {
            menu->row = row;
            return;
        }
    }
}

void display_menu_adjust(
    display_menu* menu,
    int32_t delta,
    int32_t distance_option_count)
{
    if (menu == nullptr)
    {
        return;
    }
    if (menu->screen != DISPLAY_MENU_SCREEN_SETTINGS)
    {
        return;
    }

    if (menu->row == 0)
    {
        menu->display_index = wrap_menu_index(menu->display_index, delta, menu->display_count);
        menu->display_dirty = 1u;
    }
    else if (menu->row == 1 && menu->mode_count > 0)
    {
        menu->mode_index = wrap_menu_index(menu->mode_index, delta, menu->mode_count);
        menu->display_dirty = 1u;
    }
    else if (menu->row == 2)
    {
        menu->fullscreen = toggled_flag(menu->fullscreen);
        menu->display_dirty = 1u;
    }
    else if (menu->row == 3)
    {
        menu->present_mode_index = wrap_menu_index(
            menu->present_mode_index,
            delta,
            DISPLAY_MENU_PRESENT_MODE_COUNT);
    }
    else if (menu->row == 4)
    {
        menu->render_distance_index = wrap_menu_index(menu->render_distance_index, delta, distance_option_count);
    }
    else if (menu->row == 5)
    {
        menu->fog_enabled = toggled_flag(menu->fog_enabled);
    }
    else if (menu->row == 6)
    {
        menu->clouds_enabled = toggled_flag(menu->clouds_enabled);
    }
    else if (menu->row == 7)
    {
        menu->sky_gradient_enabled = toggled_flag(menu->sky_gradient_enabled);
    }
    else if (menu->row == 8)
    {
        menu->stars_enabled = toggled_flag(menu->stars_enabled);
    }
    else if (menu->row == 9)
    {
        menu->sun_enabled = toggled_flag(menu->sun_enabled);
    }
    else if (menu->row == 10)
    {
        menu->moon_enabled = toggled_flag(menu->moon_enabled);
    }
    else if (menu->row == 11)
    {
        menu->pom_enabled = toggled_flag(menu->pom_enabled);
    }
    else if (menu->row == 12)
    {
        menu->pbr_enabled = toggled_flag(menu->pbr_enabled);
    }
}

int32_t display_menu_hit_row(
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y)
{
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        return -1;
    }

    const float base_scale = max_float(
        static_cast<float>(viewport_width) / kUiReferenceWidth,
        static_cast<float>(viewport_height) / kUiReferenceHeight);
    const float scale = base_scale * kUiScale;
    const int32_t font_scale = fit_panel_font_scale(
        max_int(1, static_cast<int32_t>(scale + 0.5f)),
        DISPLAY_MENU_ROW_COUNT,
        viewport_height);
    const int32_t padding = 4 * font_scale;
    const int32_t content_width = kTextColumns * 4 * font_scale - font_scale;
    const int32_t content_height = DISPLAY_MENU_ROW_COUNT * 6 * font_scale - font_scale;
    const int32_t panel_width = content_width + padding * 2;
    const int32_t panel_height = content_height + padding * 2;
    const int32_t panel_min_x = (viewport_width - panel_width) / 2;
    const int32_t panel_min_y = (viewport_height - panel_height) / 2;
    const int32_t text_origin_y = panel_min_y + padding;
    const int32_t line_advance = 6 * font_scale;

    const int32_t mouse_x = static_cast<int32_t>(x);
    const int32_t mouse_y = viewport_height - 1 - static_cast<int32_t>(y);
    if (mouse_x < panel_min_x || mouse_x >= panel_min_x + panel_width ||
        mouse_y < panel_min_y || mouse_y >= panel_min_y + panel_height)
    {
        return -1;
    }

    if (mouse_y < text_origin_y - font_scale ||
        mouse_y >= text_origin_y + content_height + font_scale)
    {
        return -1;
    }

    const int32_t row_from_bottom = (mouse_y - text_origin_y + font_scale) / line_advance;
    const int32_t row = DISPLAY_MENU_ROW_COUNT - 1 - row_from_bottom;
    if (row < 0 || row >= DISPLAY_MENU_ROW_COUNT)
    {
        return -1;
    }

    return row;
}

void display_menu_request_apply(display_menu* menu)
{
    if (menu == nullptr)
    {
        return;
    }

    if (menu->display_index < 0 || menu->display_index >= menu->display_count ||
        menu->mode_index < 0 || menu->mode_index >= menu->mode_count)
    {
        return;
    }

    menu->apply_requested = 1u;
    display_menu_close(menu);
}
