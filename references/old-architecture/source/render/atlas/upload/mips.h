#pragma once

#include <SDL3/SDL.h>

typedef enum main_render_atlas_mip_kind
{
    MAIN_RENDER_ATLAS_MIP_ALBEDO,
    MAIN_RENDER_ATLAS_MIP_LABPBR_NORMAL,
    MAIN_RENDER_ATLAS_MIP_LABPBR_SPECULAR,
} main_render_atlas_mip_kind_t;

void main_render_atlas_pack_layer_mips(
    Uint8* data,
    Uint32* offset,
    Uint8* current,
    Uint8* next,
    Uint32 mip_levels,
    main_render_atlas_mip_kind_t kind);
