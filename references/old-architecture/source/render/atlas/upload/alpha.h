#pragma once

#include <SDL3/SDL.h>

auto main_render_atlas_alpha_coverage_internal(const Uint8* pixels, int pixel_count) -> float;

void main_render_atlas_preserve_alpha_coverage_internal(Uint8* pixels, int pixel_count, float target_coverage);
