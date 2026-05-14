#include "render/atlas/upload/pixels.h"

#include "render/atlas/upload/alpha.h"
#include "render/atlas/upload/filter.h"

auto main_render_atlas_alpha_coverage(const Uint8* pixels, int pixel_count) -> float
{
    return main_render_atlas_alpha_coverage_internal(pixels, pixel_count);
}

void main_render_atlas_preserve_alpha_coverage(Uint8* pixels, int pixel_count, float target_coverage)
{
    main_render_atlas_preserve_alpha_coverage_internal(pixels, pixel_count, target_coverage);
}

void main_render_atlas_dilate_transparent_rgb(Uint8* pixels, int size)
{
    main_render_atlas_dilate_transparent_rgb_internal(pixels, size);
}

void main_render_atlas_downsample_tile_rgba(const Uint8* src, int src_size, Uint8* dst)
{
    main_render_atlas_downsample_tile_rgba_internal(src, src_size, dst);
}
