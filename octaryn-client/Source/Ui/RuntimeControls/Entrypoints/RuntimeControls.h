#pragma once

#include "DisplayCatalog.h"
#include "DisplayMenu.h"

#include <stdint.h>

#if defined(RUNTIME_CONTROLS_USE_SDL3)
#include <SDL3/SDL.h>
#else
typedef struct SDL_Event SDL_Event;
typedef struct SDL_Window SDL_Window;
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    RUNTIME_CONTROLS_EVENT_CAPTURED = 1u << 0u,
    RUNTIME_CONTROLS_FULLSCREEN_TOGGLED = 1u << 1u,
    RUNTIME_CONTROLS_DEBUG_TOGGLED = 1u << 2u,
    RUNTIME_CONTROLS_MENU_OPENED = 1u << 3u,
    RUNTIME_CONTROLS_MENU_CLOSED = 1u << 4u,
    RUNTIME_CONTROLS_MENU_APPLIED = 1u << 5u,
    RUNTIME_CONTROLS_QUIT_REQUESTED = 1u << 6u,
    RUNTIME_CONTROLS_MENU_ACTION = 1u << 7u,
};

typedef struct runtime_controls
{
    uint8_t debug_overlay_enabled;
    uint8_t restore_relative_mouse_after_ui;
    uint8_t fog_enabled;
    uint8_t clouds_enabled;
    uint8_t sky_gradient_enabled;
    uint8_t stars_enabled;
    uint8_t sun_enabled;
    uint8_t moon_enabled;
    uint8_t pom_enabled;
    uint8_t pbr_enabled;
    uint8_t session_active;
    uint8_t camera_mode;
    int32_t present_mode_index;
    int32_t render_distance;
    display_catalog display_catalog;
    display_menu display_menu;
} runtime_controls;

void runtime_controls_init(runtime_controls* controls);
uint8_t runtime_controls_ui_active(const runtime_controls* controls);
void runtime_controls_refresh_menu(
    runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height);
void runtime_controls_sync_relative_mouse(
    runtime_controls* controls,
    SDL_Window* window);
uint32_t runtime_controls_handle_event(
    runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event,
    int32_t viewport_width,
    int32_t viewport_height);

#ifdef __cplusplus
}
#endif
