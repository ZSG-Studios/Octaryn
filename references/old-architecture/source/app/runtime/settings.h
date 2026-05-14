#pragma once

#include <SDL3/SDL.h>

typedef struct main_runtime_settings
{
    SDL_Window* window;
    bool* fog_enabled;
    bool* clouds_enabled;
    bool* sky_gradient_enabled;
    bool* stars_enabled;
    bool* sun_enabled;
    bool* moon_enabled;
    bool* pom_enabled;
    bool* pbr_enabled;
    int* startup_render_distance;
    int* worldgen_worker_count;
}
main_runtime_settings_t;

void main_runtime_settings_init(main_runtime_settings_t* settings, SDL_Window* window, bool* fog_enabled,
                                bool* clouds_enabled, bool* sky_gradient_enabled, bool* stars_enabled,
                                bool* sun_enabled, bool* moon_enabled, bool* pom_enabled, bool* pbr_enabled,
                                int* startup_render_distance, int* worldgen_worker_count);
void main_runtime_settings_refresh_worker_options(int* options, int* option_count, int capacity);
void main_runtime_settings_load(main_runtime_settings_t* settings);
void main_runtime_settings_persist(const main_runtime_settings_t* settings);
