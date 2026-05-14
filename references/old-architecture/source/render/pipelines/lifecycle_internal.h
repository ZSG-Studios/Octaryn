#pragma once

#include "render/pipelines/internal.h"

bool main_pipelines_init_graphics(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format,
                                  SDL_GPUTextureFormat swapchain_format);
bool main_pipelines_init_compute(main_pipelines_t* pipelines, SDL_GPUDevice* device);

void main_pipelines_release_graphics(main_pipelines_t* pipelines, SDL_GPUDevice* device);
void main_pipelines_release_compute(main_pipelines_t* pipelines, SDL_GPUDevice* device);
