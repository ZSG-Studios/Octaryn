#include "render/atlas/upload/pack.h"

#include "render/atlas/texture.h"
#include "render/atlas/upload/copy.h"
#include "render/atlas/upload/mips.h"

void main_render_atlas_pack_upload_data(const SDL_Surface* surface,
                                        Uint8* data,
                                        Uint32 layer_count,
                                        Uint32 mip_levels,
                                        main_render_atlas_mip_kind_t kind)
{
    Uint8 current[MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4] = {0};
    Uint8 next[MAIN_RENDER_ATLAS_BLOCK_WIDTH * MAIN_RENDER_ATLAS_BLOCK_WIDTH * 4] = {0};
    Uint32 offset = 0;
    for (Uint32 layer = 0; layer < layer_count; ++layer)
    {
        main_render_atlas_copy_base_layer(surface, layer, current);
        main_render_atlas_pack_layer_mips(data, &offset, current, next, mip_levels, kind);
    }
}
