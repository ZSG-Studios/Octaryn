#pragma once

#include "render/pipelines/pipelines.h"

bool main_pipelines_run_init(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                             SDL_GPUTextureFormat depth_format,
                             SDL_GPUTextureFormat swapchain_format);
void main_pipelines_run_destroy(main_pipelines_t* pipelines, SDL_GPUDevice* device);
