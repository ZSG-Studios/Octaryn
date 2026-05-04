#pragma once

#include "DisplayCatalog.h"
#include "octaryn_client_display_menu.h"

#include <stdint.h>

#if defined(OCTARYN_CLIENT_RUNTIME_CONTROLS_USE_SDL3)
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
    OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED = 1u << 0u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_FULLSCREEN_TOGGLED = 1u << 1u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_DEBUG_TOGGLED = 1u << 2u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_OPENED = 1u << 3u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_CLOSED = 1u << 4u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_APPLIED = 1u << 5u,
    OCTARYN_CLIENT_RUNTIME_CONTROLS_QUIT_REQUESTED = 1u << 6u,
};

typedef struct octaryn_client_runtime_controls
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
    int32_t present_mode_index;
    int32_t render_distance;
    display_catalog display_catalog;
    octaryn_client_display_menu display_menu;
} octaryn_client_runtime_controls;

void octaryn_client_runtime_controls_init(octaryn_client_runtime_controls* controls);
uint8_t octaryn_client_runtime_controls_ui_active(const octaryn_client_runtime_controls* controls);
void octaryn_client_runtime_controls_refresh_menu(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height);
void octaryn_client_runtime_controls_sync_relative_mouse(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window);
uint32_t octaryn_client_runtime_controls_handle_event(
    octaryn_client_runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event,
    int32_t viewport_width,
    int32_t viewport_height);

#ifdef __cplusplus
}
#endif
