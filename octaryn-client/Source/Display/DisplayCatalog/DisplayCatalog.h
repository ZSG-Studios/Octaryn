#pragma once

#include <stdint.h>

#if defined(DISPLAY_CATALOG_USE_SDL3)
#include <SDL3/SDL.h>
#else
typedef uint32_t SDL_DisplayID;
typedef struct SDL_Window SDL_Window;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_CATALOG_DISPLAY_CAPACITY 16
#define DISPLAY_CATALOG_MODE_CAPACITY 64
#define DISPLAY_CATALOG_NAME_CAPACITY 128

typedef struct display_catalog_display
{
    SDL_DisplayID id;
    int32_t index;
    char name[DISPLAY_CATALOG_NAME_CAPACITY];
} display_catalog_display;

typedef struct display_catalog_mode
{
    int32_t width;
    int32_t height;
    int32_t pixel_width;
    int32_t pixel_height;
    float pixel_density;
    float refresh_rate;
} display_catalog_mode;

typedef struct display_catalog
{
    int32_t display_count;
    int32_t display_index;
    int32_t mode_count;
    int32_t mode_index;
    display_catalog_display displays[DISPLAY_CATALOG_DISPLAY_CAPACITY];
    display_catalog_mode modes[DISPLAY_CATALOG_MODE_CAPACITY];
} display_catalog;

int32_t display_catalog_mode_pixel_width(int32_t width, float pixel_density);
int32_t display_catalog_mode_pixel_height(int32_t height, float pixel_density);
int32_t display_catalog_compare_modes(
    const display_catalog_mode* left,
    const display_catalog_mode* right);
void display_catalog_refresh_displays(
    display_catalog* catalog,
    SDL_Window* window);
void display_catalog_refresh_modes(
    display_catalog* catalog,
    int32_t display_index,
    int32_t current_pixel_width,
    int32_t current_pixel_height);
void display_catalog_refresh(
    display_catalog* catalog,
    SDL_Window* window,
    int32_t current_pixel_width,
    int32_t current_pixel_height);
int32_t display_catalog_find_display_index(
    const display_catalog* catalog,
    SDL_DisplayID display);
int32_t display_catalog_find_mode_index(
    const display_catalog* catalog,
    int32_t pixel_width,
    int32_t pixel_height);

#ifdef __cplusplus
}
#endif
