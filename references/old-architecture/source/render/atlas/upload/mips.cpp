#include "render/atlas/upload/mips.h"

#include "render/atlas/texture.h"
#include "render/atlas/upload/pixels.h"

namespace {

auto average_channel(const Uint8* src, int src_size, int x, int y, int channel) -> Uint8
{
    int sum = 0;
    for (int oy = 0; oy < 2; ++oy)
    {
        const int sy = SDL_min(y * 2 + oy, src_size - 1);
        for (int ox = 0; ox < 2; ++ox)
        {
            const int sx = SDL_min(x * 2 + ox, src_size - 1);
            sum += src[(sy * src_size + sx) * 4 + channel];
        }
    }
    return static_cast<Uint8>((sum + 2) / 4);
}

void downsample_labpbr_normal(const Uint8* src, int src_size, Uint8* dst)
{
    const int dst_size = src_size > 1 ? src_size / 2 : 1;
    for (int y = 0; y < dst_size; ++y)
    {
        for (int x = 0; x < dst_size; ++x)
        {
            float xy[2] = {0.0f, 0.0f};
            int ao_sum = 0;
            int height_sum = 0;
            for (int oy = 0; oy < 2; ++oy)
            {
                const int sy = SDL_min(y * 2 + oy, src_size - 1);
                for (int ox = 0; ox < 2; ++ox)
                {
                    const int sx = SDL_min(x * 2 + ox, src_size - 1);
                    const Uint8* sample = src + (sy * src_size + sx) * 4;
                    xy[0] += static_cast<float>(sample[0]) / 255.0f * 2.0f - 1.0f;
                    xy[1] += static_cast<float>(sample[1]) / 255.0f * 2.0f - 1.0f;
                    ao_sum += sample[2];
                    height_sum += sample[3];
                }
            }
            xy[0] *= 0.25f;
            xy[1] *= 0.25f;
            const float xy_len = SDL_sqrtf(xy[0] * xy[0] + xy[1] * xy[1]);
            if (xy_len > 0.98f)
            {
                xy[0] *= 0.98f / xy_len;
                xy[1] *= 0.98f / xy_len;
            }
            Uint8* out = dst + (y * dst_size + x) * 4;
            out[0] = static_cast<Uint8>(SDL_roundf(SDL_clamp(xy[0] * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f));
            out[1] = static_cast<Uint8>(SDL_roundf(SDL_clamp(xy[1] * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f));
            out[2] = static_cast<Uint8>((ao_sum + 2) / 4);
            out[3] = static_cast<Uint8>((height_sum + 2) / 4);
        }
    }
}

auto discrete_average_labpbr_f0(const Uint8* src, int src_size, int x, int y) -> Uint8
{
    int metal_sum = 0;
    int metal_count = 0;
    int dielectric_sum = 0;
    int dielectric_count = 0;
    for (int oy = 0; oy < 2; ++oy)
    {
        const int sy = SDL_min(y * 2 + oy, src_size - 1);
        for (int ox = 0; ox < 2; ++ox)
        {
            const int sx = SDL_min(x * 2 + ox, src_size - 1);
            const Uint8 value = src[(sy * src_size + sx) * 4 + 1];
            if (value >= 230u)
            {
                metal_sum += value;
                metal_count += 1;
            }
            else
            {
                dielectric_sum += value;
                dielectric_count += 1;
            }
        }
    }
    if (metal_count > dielectric_count)
    {
        return static_cast<Uint8>((metal_sum + metal_count / 2) / metal_count);
    }
    return static_cast<Uint8>((dielectric_sum + SDL_max(dielectric_count, 1) / 2) / SDL_max(dielectric_count, 1));
}

auto discrete_average_labpbr_porosity_or_subsurface(const Uint8* src, int src_size, int x, int y) -> Uint8
{
    int porous_sum = 0;
    int porous_count = 0;
    int subsurface_sum = 0;
    int subsurface_count = 0;
    for (int oy = 0; oy < 2; ++oy)
    {
        const int sy = SDL_min(y * 2 + oy, src_size - 1);
        for (int ox = 0; ox < 2; ++ox)
        {
            const int sx = SDL_min(x * 2 + ox, src_size - 1);
            const Uint8 value = src[(sy * src_size + sx) * 4 + 2];
            if (value >= 65u)
            {
                subsurface_sum += value;
                subsurface_count += 1;
            }
            else
            {
                porous_sum += value;
                porous_count += 1;
            }
        }
    }
    if (subsurface_count > porous_count)
    {
        return static_cast<Uint8>((subsurface_sum + subsurface_count / 2) / subsurface_count);
    }
    return static_cast<Uint8>((porous_sum + SDL_max(porous_count, 1) / 2) / SDL_max(porous_count, 1));
}

auto discrete_average_labpbr_emission(const Uint8* src, int src_size, int x, int y) -> Uint8
{
    int emission_sum = 0;
    int emission_count = 0;
    for (int oy = 0; oy < 2; ++oy)
    {
        const int sy = SDL_min(y * 2 + oy, src_size - 1);
        for (int ox = 0; ox < 2; ++ox)
        {
            const int sx = SDL_min(x * 2 + ox, src_size - 1);
            const Uint8 value = src[(sy * src_size + sx) * 4 + 3];
            if (value < 255u)
            {
                emission_sum += value;
                emission_count += 1;
            }
        }
    }
    if (emission_count == 0)
    {
        return 255u;
    }
    return static_cast<Uint8>((emission_sum + emission_count / 2) / emission_count);
}

void downsample_labpbr_specular(const Uint8* src, int src_size, Uint8* dst)
{
    const int dst_size = src_size > 1 ? src_size / 2 : 1;
    for (int y = 0; y < dst_size; ++y)
    {
        for (int x = 0; x < dst_size; ++x)
        {
            Uint8* out = dst + (y * dst_size + x) * 4;
            out[0] = average_channel(src, src_size, x, y, 0);
            out[1] = discrete_average_labpbr_f0(src, src_size, x, y);
            out[2] = discrete_average_labpbr_porosity_or_subsurface(src, src_size, x, y);
            out[3] = discrete_average_labpbr_emission(src, src_size, x, y);
        }
    }
}

void downsample_data_rgba(const Uint8* src, int src_size, Uint8* dst, main_render_atlas_mip_kind_t kind)
{
    if (kind == MAIN_RENDER_ATLAS_MIP_LABPBR_NORMAL)
    {
        downsample_labpbr_normal(src, src_size, dst);
        return;
    }
    if (kind == MAIN_RENDER_ATLAS_MIP_LABPBR_SPECULAR)
    {
        downsample_labpbr_specular(src, src_size, dst);
    }
}

} // namespace

void main_render_atlas_pack_layer_mips(
    Uint8* data,
    Uint32* offset,
    Uint8* current,
    Uint8* next,
    Uint32 mip_levels,
    main_render_atlas_mip_kind_t kind)
{
    float target_coverage = 0.0f;
    if (kind == MAIN_RENDER_ATLAS_MIP_ALBEDO)
    {
        main_render_atlas_dilate_transparent_rgb(current, MAIN_RENDER_ATLAS_BLOCK_WIDTH);
        target_coverage =
            main_render_atlas_alpha_coverage(current, MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH);
    }

    Uint8* level_pixels = current;
    Uint8* next_pixels = next;
    int size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    for (Uint32 level = 0; level < mip_levels; ++level)
    {
        const Uint32 byte_count = static_cast<Uint32>(size * size * 4);
        SDL_memcpy(data + *offset, level_pixels, byte_count);
        *offset += byte_count;
        if (level + 1 < mip_levels)
        {
            const int next_size = size > 1 ? size / 2 : 1;
            if (kind == MAIN_RENDER_ATLAS_MIP_ALBEDO)
            {
                main_render_atlas_downsample_tile_rgba(level_pixels, size, next_pixels);
                main_render_atlas_dilate_transparent_rgb(next_pixels, next_size);
                main_render_atlas_preserve_alpha_coverage(next_pixels, next_size * next_size, target_coverage);
            }
            else
            {
                downsample_data_rgba(level_pixels, size, next_pixels, kind);
            }
            Uint8* swap = level_pixels;
            level_pixels = next_pixels;
            next_pixels = swap;
            size = next_size;
        }
    }
}
