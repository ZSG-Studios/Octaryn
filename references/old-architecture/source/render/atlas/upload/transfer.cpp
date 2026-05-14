#include "render/atlas/upload/transfer.h"

#include "render/atlas/texture.h"

auto main_render_atlas_transfer_to_gpu(SDL_GPUDevice* device,
                                       SDL_GPUTexture* atlas_texture,
                                       SDL_GPUTransferBuffer* buffer,
                                       Uint32 layer_count,
                                       Uint32 mip_levels) -> bool
{
    SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cbuf)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }

    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cbuf);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTextureTransferInfo info = {};
    SDL_GPUTextureRegion region = {};
    info.transfer_buffer = buffer;
    region.texture = atlas_texture;
    region.d = 1;
    Uint32 offset = 0;
    for (Uint32 layer = 0; layer < layer_count; ++layer)
    {
        int size = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
        for (Uint32 level = 0; level < mip_levels; ++level)
        {
            info.offset = offset;
            info.pixels_per_row = static_cast<Uint32>(size);
            info.rows_per_layer = static_cast<Uint32>(size);
            region.mip_level = level;
            region.layer = layer;
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
    }

    SDL_EndGPUCopyPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(cbuf))
    {
        SDL_Log("Failed to submit atlas upload command buffer: %s", SDL_GetError());
        return false;
    }
    return true;
}
