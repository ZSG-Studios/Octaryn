#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"

#define MAIN_RENDER_ATLAS_ANIMATION_CAPACITY 64
#define MAIN_RENDER_ATLAS_ANIMATION_FRAME_CAPACITY 256

typedef struct main_render_atlas_animation
{
    Uint32 layer;
    Uint32 first_frame;
    Uint32 frame_count;
    Uint32 frame_ticks[MAIN_RENDER_ATLAS_ANIMATION_FRAME_CAPACITY];
    Uint32 total_ticks;
    Uint32 active_frame;
}
main_render_atlas_animation_t;

typedef struct main_render_resources
{
    SDL_Surface* atlas_surface;
    SDL_Surface* atlas_normal_surface;
    SDL_Surface* atlas_specular_surface;
    SDL_Surface* atlas_animation_surface;
    SDL_GPUTexture* atlas_texture;
    SDL_GPUTexture* atlas_normal_texture;
    SDL_GPUTexture* atlas_specular_texture;
    SDL_GPUTexture* depth_texture;
    SDL_GPUTexture* color_texture;
    SDL_GPUTexture* position_texture;
    SDL_GPUTexture* material_texture;
    SDL_GPUTexture* voxel_texture;
    SDL_GPUTexture* composite_texture;
    SDL_GPUTexture* imgui_texture;
    SDL_GPUSampler* atlas_sampler;
    SDL_GPUSampler* cutout_sampler;
    SDL_GPUSampler* nearest_sampler;
    SDL_GPUTextureFormat color_format;
    SDL_GPUTextureFormat depth_format;
    SDL_GPUTextureFormat present_format;
    Uint32 window_width;
    Uint32 window_height;
    Uint32 render_width;
    Uint32 render_height;
    Uint32 atlas_animation_count;
    main_render_atlas_animation_t atlas_animations[MAIN_RENDER_ATLAS_ANIMATION_CAPACITY];
}
main_render_resources_t;

bool main_render_resources_init(main_render_resources_t* resources, SDL_GPUDevice* device,
                                SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format,
                                SDL_GPUTextureFormat present_format);
bool main_render_resources_resize(main_render_resources_t* resources, SDL_GPUDevice* device, int width, int height);
void main_render_resources_set_window_icon(const main_render_resources_t* resources, SDL_Window* window, block_t block);
void main_render_resources_destroy(main_render_resources_t* resources, SDL_GPUDevice* device);
