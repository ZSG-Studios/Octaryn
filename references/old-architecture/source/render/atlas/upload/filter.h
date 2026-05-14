#pragma once

#include <SDL3/SDL.h>

void main_render_atlas_dilate_transparent_rgb_internal(Uint8* pixels, int size);

void main_render_atlas_downsample_tile_rgba_internal(const Uint8* src, int src_size, Uint8* dst);
