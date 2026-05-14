#include "render/atlas/upload/copy.h"

#include "render/atlas/texture.h"

void main_render_atlas_copy_base_layer(
    const SDL_Surface* surface,
    Uint32 layer,
    Uint8* dst)
{
    for (int y = 0; y < MAIN_RENDER_ATLAS_BLOCK_WIDTH; ++y)
    {
        const Uint8* src = static_cast<const Uint8*>(surface->pixels)
                         + static_cast<size_t>(y) * static_cast<size_t>(surface->pitch)
                         + static_cast<size_t>(layer) * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4;
        SDL_memcpy(dst + y * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4, src, static_cast<size_t>(MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4));
    }
}
