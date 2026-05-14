#pragma once

#include <SDL3/SDL.h>

typedef struct main_lighting_tuning
{
    bool visible;
    Uint32 fog_enabled;
    float fog_distance;
    float skylight_floor;
    float ambient_strength;
    float sun_strength;
    float sun_fallback_strength;
}
main_lighting_tuning_t;

bool main_imgui_lighting_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format);
bool main_imgui_lighting_begin_frame(main_lighting_tuning_t* tuning);
bool main_imgui_lighting_wants_capture(void);
void main_imgui_lighting_process_event(const SDL_Event* event);
void main_imgui_lighting_render(SDL_GPUCommandBuffer* cbuf, SDL_GPUTexture* target_texture);
void main_imgui_lighting_shutdown(void);
