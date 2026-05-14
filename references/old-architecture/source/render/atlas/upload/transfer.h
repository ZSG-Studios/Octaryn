#pragma once

#include <SDL3/SDL.h>

auto main_render_atlas_transfer_to_gpu(SDL_GPUDevice* device,
                                       SDL_GPUTexture* atlas_texture,
                                       SDL_GPUTransferBuffer* buffer,
                                       Uint32 layer_count,
                                       Uint32 mip_levels) -> bool;
