#pragma once

#include <SDL3/SDL.h>

#include "render/resources/resources.h"
#include "render/atlas/upload/mips.h"

auto main_render_atlas_upload(main_render_resources_t* resources,
                              SDL_GPUDevice* device,
                              Uint32 layer_count,
                              Uint32 mip_levels) -> bool;

auto main_render_atlas_upload_texture(const SDL_Surface* surface,
                                      SDL_GPUTexture* texture,
                                      SDL_GPUDevice* device,
                                      Uint32 layer_count,
                                      Uint32 mip_levels,
                                      main_render_atlas_mip_kind_t kind,
                                      const char* label) -> bool;
