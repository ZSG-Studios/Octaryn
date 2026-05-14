#include "render/atlas/upload/alpha.h"

#include "render/atlas/texture.h"

namespace {
auto main_render_atlas_alpha_coverage_for_scale(const Uint8* pixels, int pixel_count, float scale) -> float
{
    int covered = 0;
    for (int i = 0; i < pixel_count; ++i)
    {
        float alpha = static_cast<float>(pixels[i * 4 + 3]) * scale;
        if (alpha > 255.0f)
        {
            alpha = 255.0f;
        }
        if (alpha >= MAIN_RENDER_ATLAS_ALPHA_CUTOFF_U8)
        {
            covered += 1;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(pixel_count);
}

void main_render_atlas_apply_alpha_scale(Uint8* pixels, int pixel_count, float scale)
{
    for (int i = 0; i < pixel_count; ++i)
    {
        float alpha = static_cast<float>(pixels[i * 4 + 3]) * scale;
        if (alpha > 255.0f)
        {
            alpha = 255.0f;
        }
        pixels[i * 4 + 3] = static_cast<Uint8>(SDL_roundf(alpha));
    }
}

} // namespace

auto main_render_atlas_alpha_coverage_internal(const Uint8* pixels, int pixel_count) -> float
{
    int covered = 0;
    for (int i = 0; i < pixel_count; ++i)
    {
        if (pixels[i * 4 + 3] >= MAIN_RENDER_ATLAS_ALPHA_CUTOFF_U8)
        {
            covered += 1;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(pixel_count);
}

void main_render_atlas_preserve_alpha_coverage_internal(Uint8* pixels, int pixel_count, float target_coverage)
{
    if (target_coverage <= 0.0f || target_coverage >= 1.0f)
    {
        return;
    }

    float low = 0.0f;
    float high = 1.0f;
    while (main_render_atlas_alpha_coverage_for_scale(pixels, pixel_count, high) < target_coverage && high < 16.0f)
    {
        high *= 2.0f;
    }
    for (int i = 0; i < 10; ++i)
    {
        const float mid = (low + high) * 0.5f;
        if (main_render_atlas_alpha_coverage_for_scale(pixels, pixel_count, mid) < target_coverage)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    main_render_atlas_apply_alpha_scale(pixels, pixel_count, high);
}
