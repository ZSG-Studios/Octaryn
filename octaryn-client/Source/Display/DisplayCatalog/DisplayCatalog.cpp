#include "DisplayCatalog.h"

#if defined(DISPLAY_CATALOG_USE_SDL3)
#include <cstdio>
#endif

namespace {

auto mode_area(const display_catalog_mode& mode) -> uint64_t
{
    return static_cast<uint64_t>(mode.pixel_width) * static_cast<uint64_t>(mode.pixel_height);
}

void clear_display(display_catalog_display* display)
{
    if (display == nullptr)
    {
        return;
    }

    *display = {};
    display->index = -1;
}

#if defined(DISPLAY_CATALOG_USE_SDL3)

auto valid_mode_size(const display_catalog_mode& mode) -> bool
{
    return mode.pixel_width > 0 && mode.pixel_height > 0;
}

void sort_modes(display_catalog* catalog)
{
    for (int32_t index = 1; index < catalog->mode_count; ++index)
    {
        const display_catalog_mode mode = catalog->modes[index];
        int32_t previous = index - 1;
        while (previous >= 0 && display_catalog_compare_modes(&mode, &catalog->modes[previous]) < 0)
        {
            catalog->modes[previous + 1] = catalog->modes[previous];
            --previous;
        }
        catalog->modes[previous + 1] = mode;
    }
}

void select_mode_index(display_catalog* catalog, int32_t pixel_width, int32_t pixel_height)
{
    catalog->mode_index = display_catalog_find_mode_index(catalog, pixel_width, pixel_height);
    if (catalog->mode_index < 0 && catalog->mode_count > 0)
    {
        catalog->mode_index = 0;
    }
}

auto from_sdl_mode(const SDL_DisplayMode* mode) -> display_catalog_mode
{
    if (mode == nullptr)
    {
        return {};
    }

    display_catalog_mode output{};
    output.width = mode->w;
    output.height = mode->h;
    output.pixel_density = mode->pixel_density;
    output.refresh_rate = mode->refresh_rate;
    output.pixel_width = display_catalog_mode_pixel_width(mode->w, mode->pixel_density);
    output.pixel_height = display_catalog_mode_pixel_height(mode->h, mode->pixel_density);
    return output;
}

auto resolve_window_display_index(const display_catalog* catalog, SDL_Window* window) -> int32_t
{
    SDL_DisplayID display = window != nullptr ? SDL_GetDisplayForWindow(window) : 0;
    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }

    const int32_t index = display_catalog_find_display_index(catalog, display);
    return index >= 0 ? index : 0;
}

void copy_display_name(display_catalog_display* display)
{
    if (display == nullptr)
    {
        return;
    }

    display->name[0] = '\0';
    const char* name = display->id != 0 ? SDL_GetDisplayName(display->id) : nullptr;
    if (name != nullptr)
    {
        std::snprintf(
            display->name,
            static_cast<size_t>(DISPLAY_CATALOG_NAME_CAPACITY),
            "%s",
            name);
    }
}

void add_or_replace_mode(
    display_catalog* catalog,
    const display_catalog_mode& candidate)
{
    if (!valid_mode_size(candidate))
    {
        return;
    }

    for (int32_t index = 0; index < catalog->mode_count; ++index)
    {
        display_catalog_mode& existing = catalog->modes[index];
        if (existing.pixel_width == candidate.pixel_width && existing.pixel_height == candidate.pixel_height)
        {
            if (candidate.refresh_rate > existing.refresh_rate)
            {
                existing = candidate;
            }
            return;
        }
    }

    if (catalog->mode_count < DISPLAY_CATALOG_MODE_CAPACITY)
    {
        catalog->modes[catalog->mode_count] = candidate;
        ++catalog->mode_count;
    }
}

#endif

} // namespace

int32_t display_catalog_mode_pixel_width(int32_t width, float pixel_density)
{
    if (width <= 0 || pixel_density <= 0.0f)
    {
        return 0;
    }

    return static_cast<int32_t>(static_cast<float>(width) * pixel_density + 0.5f);
}

int32_t display_catalog_mode_pixel_height(int32_t height, float pixel_density)
{
    if (height <= 0 || pixel_density <= 0.0f)
    {
        return 0;
    }

    return static_cast<int32_t>(static_cast<float>(height) * pixel_density + 0.5f);
}

int32_t display_catalog_compare_modes(
    const display_catalog_mode* left,
    const display_catalog_mode* right)
{
    if (left == nullptr || right == nullptr)
    {
        return left == right ? 0 : (left == nullptr ? -1 : 1);
    }

    const uint64_t left_area = mode_area(*left);
    const uint64_t right_area = mode_area(*right);
    if (left_area != right_area)
    {
        return left_area < right_area ? -1 : 1;
    }
    if (left->pixel_width != right->pixel_width)
    {
        return left->pixel_width < right->pixel_width ? -1 : 1;
    }
    if (left->pixel_height != right->pixel_height)
    {
        return left->pixel_height < right->pixel_height ? -1 : 1;
    }
    if (left->refresh_rate != right->refresh_rate)
    {
        return left->refresh_rate < right->refresh_rate ? -1 : 1;
    }

    return 0;
}

void display_catalog_refresh_displays(
    display_catalog* catalog,
    SDL_Window* window)
{
    if (catalog == nullptr)
    {
        return;
    }

    catalog->display_count = 0;
    catalog->display_index = -1;
    catalog->mode_count = 0;
    catalog->mode_index = -1;
    for (int32_t index = 0; index < DISPLAY_CATALOG_DISPLAY_CAPACITY; ++index)
    {
        clear_display(&catalog->displays[index]);
    }

#if defined(DISPLAY_CATALOG_USE_SDL3)
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (displays != nullptr)
    {
        for (int32_t index = 0;
             index < count && catalog->display_count < DISPLAY_CATALOG_DISPLAY_CAPACITY;
             ++index)
        {
            display_catalog_display& output = catalog->displays[catalog->display_count];
            output.id = displays[index];
            output.index = catalog->display_count;
            copy_display_name(&output);
            ++catalog->display_count;
        }
        SDL_free(displays);
    }

    if (catalog->display_count <= 0)
    {
        display_catalog_display& primary = catalog->displays[0];
        primary.id = SDL_GetPrimaryDisplay();
        if (primary.id != 0)
        {
            primary.index = 0;
            copy_display_name(&primary);
            catalog->display_count = 1;
        }
    }

    catalog->display_index = resolve_window_display_index(catalog, window);
#else
    (void)window;
#endif
}

void display_catalog_refresh_modes(
    display_catalog* catalog,
    int32_t display_index,
    int32_t current_pixel_width,
    int32_t current_pixel_height)
{
    if (catalog == nullptr)
    {
        return;
    }

    catalog->mode_count = 0;
    catalog->mode_index = -1;
    if (display_index < 0 || display_index >= catalog->display_count)
    {
        return;
    }
    catalog->display_index = display_index;

#if defined(DISPLAY_CATALOG_USE_SDL3)
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(catalog->displays[catalog->display_index].id, &count);
    if (modes != nullptr)
    {
        for (int32_t index = 0; index < count; ++index)
        {
            add_or_replace_mode(catalog, from_sdl_mode(modes[index]));
        }
        SDL_free(modes);
    }

    sort_modes(catalog);

    select_mode_index(catalog, current_pixel_width, current_pixel_height);
#else
    (void)current_pixel_width;
    (void)current_pixel_height;
#endif
}

void display_catalog_refresh(
    display_catalog* catalog,
    SDL_Window* window,
    int32_t current_pixel_width,
    int32_t current_pixel_height)
{
    if (catalog == nullptr)
    {
        return;
    }

    display_catalog_refresh_displays(catalog, window);

#if defined(DISPLAY_CATALOG_USE_SDL3)
    int32_t window_width = current_pixel_width;
    int32_t window_height = current_pixel_height;
    if ((window_width <= 0 || window_height <= 0) &&
        (window == nullptr || !SDL_GetWindowSizeInPixels(window, &window_width, &window_height)))
    {
        window_width = current_pixel_width;
        window_height = current_pixel_height;
    }
    display_catalog_refresh_modes(
        catalog,
        catalog->display_index,
        window_width,
        window_height);
#else
    (void)window;
    (void)current_pixel_width;
    (void)current_pixel_height;
#endif
}

int32_t display_catalog_find_display_index(
    const display_catalog* catalog,
    SDL_DisplayID display)
{
    if (catalog == nullptr || display == 0)
    {
        return -1;
    }

    for (int32_t index = 0; index < catalog->display_count; ++index)
    {
        if (catalog->displays[index].id == display)
        {
            return index;
        }
    }

    return -1;
}

int32_t display_catalog_find_mode_index(
    const display_catalog* catalog,
    int32_t pixel_width,
    int32_t pixel_height)
{
    if (catalog == nullptr || pixel_width <= 0 || pixel_height <= 0)
    {
        return -1;
    }

    for (int32_t index = 0; index < catalog->mode_count; ++index)
    {
        if (catalog->modes[index].pixel_width == pixel_width &&
            catalog->modes[index].pixel_height == pixel_height)
        {
            return index;
        }
    }

    return -1;
}
