#include "render/atlas/internal.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/asset_paths.h"
#include "render/atlas/texture.h"
#include "render/atlas/upload/data.h"
#include "render/atlas/upload/mips.h"

namespace {

constexpr Uint64 kMinecraftTickNs = 50000000ull;

auto split(const std::string& text, char separator) -> std::vector<std::string>
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator))
    {
        parts.push_back(part);
    }
    return parts;
}

auto load_rgba_surface(const char* asset_name, SDL_Surface** surface) -> bool
{
    char path[512] = {0};
    if (!asset_path_build(path, sizeof(path), asset_name))
    {
        SDL_Log("Failed to build atlas animation asset path for %s", asset_name);
        return false;
    }

    *surface = SDL_LoadPNG(path);
    if (!*surface)
    {
        SDL_Log("Failed to load atlas animation surface %s: %s", asset_name, SDL_GetError());
        return false;
    }

    if ((*surface)->format != SDL_PIXELFORMAT_RGBA32)
    {
        SDL_Surface* converted = SDL_ConvertSurface(*surface, SDL_PIXELFORMAT_RGBA32);
        if (!converted)
        {
            SDL_Log("Failed to convert atlas animation surface: %s", SDL_GetError());
            return false;
        }
        SDL_DestroySurface(*surface);
        *surface = converted;
    }
    return true;
}

auto animation_manifest_path(char* path, size_t path_size) -> bool
{
    return asset_path_build(path, path_size, "textures/atlas_anim.txt");
}

void copy_animation_frame(const SDL_Surface* surface, Uint32 frame, Uint8* pixels)
{
    const int tile_size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    const Uint8* base = static_cast<const Uint8*>(surface->pixels);
    const int src_x = static_cast<int>(frame) * tile_size;
    for (int y = 0; y < tile_size; ++y)
    {
        const Uint8* src = base + y * surface->pitch + src_x * 4;
        SDL_memcpy(pixels + y * tile_size * 4, src, static_cast<size_t>(tile_size * 4));
    }
}

auto upload_animation_frame(main_render_resources_t* resources,
                            SDL_GPUDevice* device,
                            const main_render_atlas_animation_t& animation,
                            Uint32 frame) -> bool
{
    const Uint32 mip_levels = main_render_atlas_mip_levels();
    SDL_GPUTransferBufferCreateInfo buffer_info = {};
    buffer_info.size = main_render_atlas_upload_size(1, mip_levels);
    buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    if (!buffer)
    {
        SDL_Log("Failed to create atlas animation transfer buffer: %s", SDL_GetError());
        return false;
    }

    Uint8* data = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, buffer, 0));
    if (!data)
    {
        SDL_Log("Failed to map atlas animation transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }

    Uint8 current[MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4] = {0};
    Uint8 next[MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4] = {0};
    Uint32 offset = 0;
    copy_animation_frame(resources->atlas_animation_surface, animation.first_frame + frame, current);
    main_render_atlas_pack_layer_mips(data, &offset, current, next, mip_levels, MAIN_RENDER_ATLAS_MIP_ALBEDO);
    SDL_UnmapGPUTransferBuffer(device, buffer);

    SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cbuf)
    {
        SDL_Log("Failed to acquire atlas animation command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cbuf);
    if (!pass)
    {
        SDL_Log("Failed to begin atlas animation copy pass: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }

    SDL_GPUTextureTransferInfo info = {};
    SDL_GPUTextureRegion region = {};
    info.transfer_buffer = buffer;
    region.texture = resources->atlas_texture;
    region.layer = animation.layer;
    region.d = 1;
    offset = 0;
    int size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    for (Uint32 level = 0; level < mip_levels; ++level)
    {
        info.offset = offset;
        info.pixels_per_row = static_cast<Uint32>(size);
        info.rows_per_layer = static_cast<Uint32>(size);
        region.mip_level = level;
        region.x = 0;
        region.y = 0;
        region.z = 0;
        region.w = static_cast<Uint32>(size);
        region.h = static_cast<Uint32>(size);
        SDL_UploadToGPUTexture(pass, &info, &region, 0);
        offset += static_cast<Uint32>(size * size * 4);
        if (size > 1)
        {
            size /= 2;
        }
    }

    SDL_EndGPUCopyPass(pass);
    const bool submitted = SDL_SubmitGPUCommandBuffer(cbuf);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    if (!submitted)
    {
        SDL_Log("Failed to submit atlas animation command buffer: %s", SDL_GetError());
        return false;
    }
    return true;
}

auto frame_for_tick(const main_render_atlas_animation_t& animation, Uint64 tick) -> Uint32
{
    const Uint32 local_tick = static_cast<Uint32>(tick % animation.total_ticks);
    Uint32 cursor = 0;
    for (Uint32 frame = 0; frame < animation.frame_count; ++frame)
    {
        cursor += animation.frame_ticks[frame];
        if (local_tick < cursor)
        {
            return frame;
        }
    }
    return 0;
}

} // namespace

auto main_render_atlas_load_animations_internal(main_render_resources_t* resources) -> bool
{
    char manifest_path[512] = {0};
    if (!animation_manifest_path(manifest_path, sizeof(manifest_path)))
    {
        return false;
    }

    std::ifstream manifest(manifest_path);
    if (!manifest.is_open())
    {
        return true;
    }

    if (!load_rgba_surface("textures/atlas_anim.png", &resources->atlas_animation_surface))
    {
        return false;
    }
    if (resources->atlas_animation_surface->h != MAIN_RENDER_ATLAS_BLOCK_WIDTH ||
        resources->atlas_animation_surface->w % MAIN_RENDER_ATLAS_BLOCK_WIDTH != 0)
    {
        SDL_Log("Unexpected atlas animation dimensions: %dx%d",
                resources->atlas_animation_surface->w,
                resources->atlas_animation_surface->h);
        return false;
    }

    std::string line;
    while (std::getline(manifest, line))
    {
        constexpr const char* prefix = "animation=";
        if (!line.starts_with(prefix))
        {
            continue;
        }
        if (resources->atlas_animation_count >= MAIN_RENDER_ATLAS_ANIMATION_CAPACITY)
        {
            SDL_Log("Atlas animation capacity reached; skipping remaining animation entries");
            break;
        }

        const std::vector<std::string> fields = split(line.substr(SDL_strlen(prefix)), '|');
        if (fields.size() != 5)
        {
            SDL_Log("Skipping malformed atlas animation entry: %s", line.c_str());
            continue;
        }

        main_render_atlas_animation_t animation = {};
        animation.layer = static_cast<Uint32>(std::stoul(fields[0]));
        animation.first_frame = static_cast<Uint32>(std::stoul(fields[2]));
        animation.frame_count = static_cast<Uint32>(std::stoul(fields[3]));
        if (animation.layer >= MAIN_RENDER_ATLAS_LAYER_COUNT ||
            animation.frame_count == 0 ||
            animation.frame_count > MAIN_RENDER_ATLAS_ANIMATION_FRAME_CAPACITY)
        {
            SDL_Log("Skipping invalid atlas animation entry: %s", line.c_str());
            continue;
        }

        const std::vector<std::string> ticks = split(fields[4], ',');
        if (ticks.size() != animation.frame_count)
        {
            SDL_Log("Skipping atlas animation with mismatched frame timing: %s", line.c_str());
            continue;
        }
        for (Uint32 i = 0; i < animation.frame_count; ++i)
        {
            animation.frame_ticks[i] = std::max<Uint32>(1u, static_cast<Uint32>(std::stoul(ticks[i])));
            animation.total_ticks += animation.frame_ticks[i];
        }
        const Uint32 available_frames =
            static_cast<Uint32>(resources->atlas_animation_surface->w / MAIN_RENDER_ATLAS_BLOCK_WIDTH);
        if (animation.first_frame + animation.frame_count > available_frames || animation.total_ticks == 0)
        {
            SDL_Log("Skipping atlas animation outside packed frame range: %s", line.c_str());
            continue;
        }
        animation.active_frame = UINT32_MAX;
        resources->atlas_animations[resources->atlas_animation_count++] = animation;
    }

    SDL_Log("Loaded %u atlas animation(s)", resources->atlas_animation_count);
    return true;
}

void main_render_atlas_update_animations_internal(main_render_resources_t* resources, SDL_GPUDevice* device, Uint64 ticks_ns)
{
    if (!resources->atlas_animation_surface || resources->atlas_animation_count == 0)
    {
        return;
    }

    const Uint64 minecraft_tick = ticks_ns / kMinecraftTickNs;
    for (Uint32 i = 0; i < resources->atlas_animation_count; ++i)
    {
        main_render_atlas_animation_t& animation = resources->atlas_animations[i];
        const Uint32 frame = frame_for_tick(animation, minecraft_tick);
        if (frame == animation.active_frame)
        {
            continue;
        }
        if (upload_animation_frame(resources, device, animation, frame))
        {
            animation.active_frame = frame;
        }
    }
}
