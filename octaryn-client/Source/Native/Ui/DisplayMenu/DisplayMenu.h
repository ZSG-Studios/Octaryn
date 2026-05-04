#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_MENU_ROW_COUNT 16
#define DISPLAY_MENU_APPLY_ROW 13
#define DISPLAY_MENU_CLOSE_ROW 14
#define DISPLAY_MENU_EXIT_ROW 15
#define DISPLAY_MENU_PRESENT_MODE_COUNT 3

typedef struct display_menu
{
    uint8_t active;
    uint8_t apply_requested;
    uint8_t display_dirty;
    int32_t row;
    uint8_t fullscreen;
    int32_t present_mode_index;
    uint8_t fog_enabled;
    uint8_t clouds_enabled;
    uint8_t sky_gradient_enabled;
    uint8_t stars_enabled;
    uint8_t sun_enabled;
    uint8_t moon_enabled;
    uint8_t pom_enabled;
    uint8_t pbr_enabled;
    int32_t display_count;
    int32_t display_index;
    int32_t render_distance_index;
    int32_t mode_count;
    int32_t mode_index;
} display_menu;

int32_t display_menu_mode_pixel_width(int32_t mode_width, float pixel_density);
int32_t display_menu_mode_pixel_height(int32_t mode_height, float pixel_density);
void display_menu_open(display_menu* menu);
void display_menu_close(display_menu* menu);
void display_menu_adjust(
    display_menu* menu,
    int32_t delta,
    int32_t distance_option_count);
int32_t display_menu_hit_row(
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y);
void display_menu_request_apply(display_menu* menu);

#ifdef __cplusplus
}
#endif
