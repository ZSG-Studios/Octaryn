#pragma once

#include <SDL3/SDL.h>

void app_present_startup_frame(void);
void app_render_swapchain(SDL_GPUCommandBuffer* cbuf,
                          SDL_GPUTexture* swapchain_texture,
                          SDL_GPUTexture* overlay_texture,
                          bool overlay_enabled);
