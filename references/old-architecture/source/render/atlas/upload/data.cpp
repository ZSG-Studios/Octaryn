#include "render/atlas/upload/data.h"

#include "render/atlas/texture.h"
#include "render/atlas/upload/pack.h"

auto main_render_atlas_upload_size(Uint32 layer_count, Uint32 mip_levels) -> Uint32
{
    Uint32 upload_size = 0;
    for (Uint32 layer = 0; layer < layer_count; ++layer)
    {
        int size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
        for (Uint32 level = 0; level < mip_levels; ++level)
        {
            upload_size += static_cast<Uint32>(size * size * 4);
            if (size > 1)
            {
                size /= 2;
            }
        }
    }
    return upload_size;
}

auto main_render_atlas_build_upload_data(const SDL_Surface* surface,
                                         SDL_GPUDevice* device,
                                         SDL_GPUTransferBuffer* buffer,
                                         Uint32 layer_count,
                                         Uint32 mip_levels,
                                         main_render_atlas_mip_kind_t kind) -> bool
{
    Uint8* data = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, buffer, 0));
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }

    main_render_atlas_pack_upload_data(surface, data, layer_count, mip_levels, kind);

    SDL_UnmapGPUTransferBuffer(device, buffer);
    return true;
}
