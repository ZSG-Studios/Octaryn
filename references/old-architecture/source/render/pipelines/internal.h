#pragma once

#include "render/pipelines/pipelines.h"

bool main_pipelines_create_opaque(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_opaque_sprite(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                         SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_transparent(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                       SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_water(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                 SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_lava(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_clouds(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_depth(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_sky(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                               SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_selection(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                     SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format);
bool main_pipelines_create_present(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                   SDL_GPUTextureFormat swapchain_format);

bool main_pipelines_load_compute(SDL_GPUComputePipeline** pipeline, SDL_GPUDevice* device, const char* path);
