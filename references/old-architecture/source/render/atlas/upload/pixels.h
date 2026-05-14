#pragma once

#include <SDL3/SDL.h>

auto main_render_atlas_alpha_coverage(const Uint8* pixels, int pixel_count) -> float;

void main_render_atlas_preserve_alpha_coverage(Uint8* pixels, int pixel_count, float target_coverage);

void main_render_atlas_dilate_transparent_rgb(Uint8* pixels, int size);

void main_render_atlas_downsample_tile_rgba(const Uint8* src, int src_size, Uint8* dst);
