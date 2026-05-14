#include "render/atlas/upload/filter.h"

#include "render/atlas/texture.h"

namespace {

auto srgb8_to_linear(Uint8 value) -> float
{
    const float c = static_cast<float>(value) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : SDL_powf((c + 0.055f) / 1.055f, 2.4f);
}

auto linear_to_srgb8(float value) -> Uint8
{
    value = SDL_clamp(value, 0.0f, 1.0f);
    const float c = value <= 0.0031308f ? value * 12.92f : 1.055f * SDL_powf(value, 1.0f / 2.4f) - 0.055f;
    return static_cast<Uint8>(SDL_roundf(c * 255.0f));
}

} // namespace

void main_render_atlas_dilate_transparent_rgb_internal(Uint8* pixels, int size)
{
    Uint8 source[MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4] = {0};
    SDL_memcpy(source, pixels, static_cast<size_t>(size * size * 4));
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            Uint8* pixel = pixels + (y * size + x) * 4;
            if (pixel[3] == 255u)
            {
                continue;
            }
            float sum[3] = {0.0f};
            float weight_sum = 0.0f;
            for (int oy = -1; oy <= 1; ++oy)
            {
                const int sy = y + oy;
                if (sy < 0 || sy >= size)
                {
                    continue;
                }
                for (int ox = -1; ox <= 1; ++ox)
                {
                    const int sx = x + ox;
                    if (sx < 0 || sx >= size)
                    {
                        continue;
                    }
                    const Uint8* sample = source + (sy * size + sx) * 4;
                    if (sample[3] < MAIN_RENDER_ATLAS_ALPHA_CUTOFF_U8)
                    {
                        continue;
                    }
                    const float weight = static_cast<float>(sample[3]) / 255.0f;
                    sum[0] += srgb8_to_linear(sample[0]) * weight;
                    sum[1] += srgb8_to_linear(sample[1]) * weight;
                    sum[2] += srgb8_to_linear(sample[2]) * weight;
                    weight_sum += weight;
                }
            }
            if (weight_sum > SDL_FLT_EPSILON)
            {
                pixel[0] = linear_to_srgb8(sum[0] / weight_sum);
                pixel[1] = linear_to_srgb8(sum[1] / weight_sum);
                pixel[2] = linear_to_srgb8(sum[2] / weight_sum);
            }
        }
    }
}

void main_render_atlas_downsample_tile_rgba_internal(const Uint8* src, int src_size, Uint8* dst)
{
    const int dst_size = src_size > 1 ? src_size / 2 : 1;
    for (int y = 0; y < dst_size; ++y)
    {
        for (int x = 0; x < dst_size; ++x)
        {
            float sum_alpha = 0.0f;
            float sum_red = 0.0f;
            float sum_green = 0.0f;
            float sum_blue = 0.0f;
            for (int oy = 0; oy < 2; ++oy)
            {
                const int sy = SDL_min(y * 2 + oy, src_size - 1);
                for (int ox = 0; ox < 2; ++ox)
                {
                    const int sx = SDL_min(x * 2 + ox, src_size - 1);
                    const Uint8* sample = src + (sy * src_size + sx) * 4;
                    const float alpha = static_cast<float>(sample[3]) / 255.0f;
                    sum_alpha += alpha;
                    sum_red += srgb8_to_linear(sample[0]) * alpha;
                    sum_green += srgb8_to_linear(sample[1]) * alpha;
                    sum_blue += srgb8_to_linear(sample[2]) * alpha;
                }
            }

            Uint8* out = dst + (y * dst_size + x) * 4;
            const float out_alpha = sum_alpha * 0.25f;
            out[3] = static_cast<Uint8>(SDL_roundf(out_alpha * 255.0f));
            if (sum_alpha > SDL_FLT_EPSILON)
            {
                out[0] = linear_to_srgb8((sum_red * 0.25f) / out_alpha);
                out[1] = linear_to_srgb8((sum_green * 0.25f) / out_alpha);
                out[2] = linear_to_srgb8((sum_blue * 0.25f) / out_alpha);
            }
            else
            {
                out[0] = 0;
                out[1] = 0;
                out[2] = 0;
            }
        }
    }
}
