#pragma once

#include <SDL3/SDL.h>

#include "app/player/player.h"

#define MAIN_DISPLAY_MENU_MAX_DISPLAYS 16
#define MAIN_DISPLAY_MENU_MAX_MODES 64
#define MAIN_DISPLAY_MENU_ROW_COUNT 15
#define MAIN_DISPLAY_MENU_APPLY_ROW 12
#define MAIN_DISPLAY_MENU_CLOSE_ROW 13
#define MAIN_DISPLAY_MENU_EXIT_ROW 14

typedef struct main_display_menu
{
    bool active;
    bool apply_requested;
    bool display_dirty;
    int row;
    bool fullscreen;
    bool fog_enabled;
    bool clouds_enabled;
    bool sky_gradient_enabled;
    bool stars_enabled;
    bool sun_enabled;
    bool moon_enabled;
    bool pom_enabled;
    bool pbr_enabled;
    int display_count;
    int display_index;
    int render_distance_index;
    SDL_DisplayID displays[MAIN_DISPLAY_MENU_MAX_DISPLAYS];
    int mode_count;
    int mode_index;
    SDL_DisplayMode modes[MAIN_DISPLAY_MENU_MAX_MODES];
}
main_display_menu_t;

typedef struct main_display_menu_runtime
{
    SDL_Window* window;
    player_t* player;
    bool* fog_enabled;
    bool* clouds_enabled;
    bool* sky_gradient_enabled;
    bool* stars_enabled;
    bool* sun_enabled;
    bool* moon_enabled;
    bool* pom_enabled;
    bool* pbr_enabled;
    const int* distance_options;
    int distance_option_count;
    void (*persist_settings)(void);
}
main_display_menu_runtime_t;

int main_display_menu_mode_pixel_width(const SDL_DisplayMode* mode);
int main_display_menu_mode_pixel_height(const SDL_DisplayMode* mode);
void main_display_menu_open(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);
void main_display_menu_close(main_display_menu_t* menu);
void main_display_menu_adjust(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime, int delta);
int main_display_menu_hit_row(int viewport_width, int viewport_height, float x, float y);
void main_display_menu_request_apply(main_display_menu_t* menu);
void main_display_menu_commit_if_requested(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime);
