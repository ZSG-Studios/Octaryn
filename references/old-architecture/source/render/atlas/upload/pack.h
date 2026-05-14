#pragma once

#include <SDL3/SDL.h>

#include "render/atlas/upload/mips.h"

void main_render_atlas_pack_upload_data(const SDL_Surface* surface,
                                        Uint8* data,
                                        Uint32 layer_count,
                                        Uint32 mip_levels,
                                        main_render_atlas_mip_kind_t kind);
