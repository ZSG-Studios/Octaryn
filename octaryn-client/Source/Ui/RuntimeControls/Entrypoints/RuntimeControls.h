#pragma once

#include <stdint.h>

#if defined(RUNTIME_CONTROLS_USE_SDL3)
#include <SDL3/SDL.h>
#else
typedef union SDL_Event SDL_Event;
typedef struct SDL_Window SDL_Window;
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    RUNTIME_CONTROLS_FULLSCREEN_TOGGLED = 1u << 1u,
    RUNTIME_CONTROLS_DEBUG_TOGGLED = 1u << 2u,
    RUNTIME_CONTROLS_FLIGHT_TOGGLED = 1u << 8u,
    RUNTIME_CONTROLS_ZOOM_CYCLED = 1u << 9u,
};

typedef struct runtime_controls
{
    uint8_t debug_overlay_enabled;
    uint8_t fog_enabled;
    uint8_t clouds_enabled;
    uint8_t sky_gradient_enabled;
    uint8_t stars_enabled;
    uint8_t sun_enabled;
    uint8_t moon_enabled;
    uint8_t pom_enabled;
    uint8_t pbr_enabled;
    uint8_t ray_tracing_enabled;
    uint8_t ray_tracing_available;
    uint8_t raster_sun_shadows;
    uint8_t upscaler_mode;
    uint8_t fsr_sharpening;
    float fsr_sharpness;
    float fsr_render_scale;
    uint8_t fsr_dynamic_resolution;
    float fsr_min_scale;
    float fsr_max_scale;
    uint16_t fsr_target_fps;
    uint16_t frame_cap_fps;
    uint16_t shadow_distance;
    uint16_t reflection_distance;
    uint8_t reflection_quality;
    uint8_t shadow_quality;
    int32_t present_mode_index;
} runtime_controls;

void runtime_controls_init(runtime_controls* controls);
uint32_t runtime_controls_handle_event(
    runtime_controls* controls,
    SDL_Window* window,
    SDL_Event* event);

#ifdef __cplusplus
}
#endif
