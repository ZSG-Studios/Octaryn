// Ported from upstream Octaryn 3557cbf source/render/atlas/upload/alpha.cpp
#include "AtlasPixels.h"


namespace {
auto atlas_alpha_coverage_for_scale(const Uint8* pixels, int pixel_count, float scale) -> float
{
    int covered = 0;
    for (int i = 0; i < pixel_count; ++i)
    {
        float alpha = static_cast<float>(pixels[i * 4 + 3]) * scale;
        if (alpha > 255.0f)
        {
            alpha = 255.0f;
        }
        // Search the same quantized byte values that atlas_apply_alpha_scale
        // commits; a float value of 89.3 rounds below the shader cutoff.
        if (SDL_roundf(alpha) >= ATLAS_ALPHA_CUTOFF_U8)
        {
            covered += 1;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(pixel_count);
}

void atlas_apply_alpha_scale(Uint8* pixels, int pixel_count, float scale)
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

auto atlas_alpha_coverage(const Uint8* pixels, int pixel_count) -> float
{
    int covered = 0;
    for (int i = 0; i < pixel_count; ++i)
    {
        if (pixels[i * 4 + 3] >= ATLAS_ALPHA_CUTOFF_U8)
        {
            covered += 1;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(pixel_count);
}

void atlas_preserve_alpha_coverage(Uint8* pixels, int pixel_count, float target_coverage)
{
    if (target_coverage <= 0.0f || target_coverage >= 1.0f)
    {
        return;
    }
    // Keep neutral alpha when the mip already has the requested coverage.
    // Searching for its lowest passing scale would turn opaque 255 into 90,
    // erasing most edge coverage once bilinear/trilinear filtering is applied.
    if (atlas_alpha_coverage(pixels, pixel_count) == target_coverage)
    {
        return;
    }

    float low = 0.0f;
    float high = 1.0f;
    while (atlas_alpha_coverage_for_scale(pixels, pixel_count, high) < target_coverage && high < 16.0f)
    {
        high *= 2.0f;
    }
    for (int i = 0; i < 10; ++i)
    {
        const float mid = (low + high) * 0.5f;
        if (atlas_alpha_coverage_for_scale(pixels, pixel_count, mid) < target_coverage)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    atlas_apply_alpha_scale(pixels, pixel_count, high);
}
