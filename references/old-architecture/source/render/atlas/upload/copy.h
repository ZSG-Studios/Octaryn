#pragma once

#include <SDL3/SDL.h>

void main_render_atlas_copy_base_layer(
    const SDL_Surface* surface,
    Uint32 layer,
    Uint8* dst);
