#pragma once

#include <SDL3/SDL.h>

typedef struct main_pipelines
{
    SDL_GPUGraphicsPipeline* opaque;
    SDL_GPUGraphicsPipeline* opaque_sprite;
    SDL_GPUGraphicsPipeline* transparent;
    SDL_GPUGraphicsPipeline* water;
    SDL_GPUGraphicsPipeline* lava;
    SDL_GPUGraphicsPipeline* clouds;
    SDL_GPUGraphicsPipeline* depth;
    SDL_GPUGraphicsPipeline* sky;
    SDL_GPUGraphicsPipeline* selection;
    SDL_GPUGraphicsPipeline* present;
    SDL_GPUComputePipeline* ui;
    SDL_GPUComputePipeline* composite;
}
main_pipelines_t;

bool main_pipelines_init(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                         SDL_GPUTextureFormat depth_format,
                         SDL_GPUTextureFormat swapchain_format);
void main_pipelines_destroy(main_pipelines_t* pipelines, SDL_GPUDevice* device);
