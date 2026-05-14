#pragma once

#include <SDL3/SDL.h>

#include "render/resources/resources.h"
#include "render/atlas/upload/mips.h"

auto main_render_atlas_upload_size(Uint32 layer_count, Uint32 mip_levels) -> Uint32;

auto main_render_atlas_build_upload_data(const SDL_Surface* surface,
                                         SDL_GPUDevice* device,
                                         SDL_GPUTransferBuffer* buffer,
                                         Uint32 layer_count,
                                         Uint32 mip_levels,
                                         main_render_atlas_mip_kind_t kind) -> bool;
