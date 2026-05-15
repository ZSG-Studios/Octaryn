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
#define DISPLAY_MENU_SCREEN_MAIN 0u
#define DISPLAY_MENU_SCREEN_SINGLEPLAYER 1u
#define DISPLAY_MENU_SCREEN_MULTIPLAYER 2u
#define DISPLAY_MENU_SCREEN_SETTINGS 3u
#define DISPLAY_MENU_SCREEN_INGAME 4u
#define DISPLAY_MENU_ACTION_NONE 0u
#define DISPLAY_MENU_ACTION_CREATE_WORLD 1u
#define DISPLAY_MENU_ACTION_LOAD_WORLD 2u
#define DISPLAY_MENU_ACTION_SAVE_WORLD 3u
#define DISPLAY_MENU_ACTION_CONNECT_LOCAL 4u
#define DISPLAY_MENU_ACTION_CONNECT_SERVER 5u
#define DISPLAY_MENU_ACTION_SAVE_SERVER 6u
#define DISPLAY_MENU_ACTION_DELETE_WORLD 7u
#define DISPLAY_MENU_ACTION_DISCONNECT_SESSION 8u
#define DISPLAY_MENU_SERVER_ADDRESS_SIZE 16
#define DISPLAY_MENU_SERVER_PORT_SIZE 6
#define DISPLAY_MENU_WORLD_NAME_SIZE 16

typedef struct display_menu
{
    uint8_t active;
    uint8_t apply_requested;
    uint8_t display_dirty;
    uint32_t screen;
    uint32_t action_requested;
    uint32_t world_slot;
    uint32_t editing_field;
    uint32_t status_code;
    int32_t row;
    char world_name[DISPLAY_MENU_WORLD_NAME_SIZE];
    char server_address[DISPLAY_MENU_SERVER_ADDRESS_SIZE];
    char server_port[DISPLAY_MENU_SERVER_PORT_SIZE];
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
void display_menu_move_row(display_menu* menu, int32_t delta);
uint8_t display_menu_row_selectable(const display_menu* menu, int32_t row);
int32_t display_menu_hit_row(
    int32_t viewport_width,
    int32_t viewport_height,
    float x,
    float y);
void display_menu_request_apply(display_menu* menu);

#ifdef __cplusplus
}
#endif
