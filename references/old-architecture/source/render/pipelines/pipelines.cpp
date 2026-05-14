#include "render/pipelines/pipelines.h"

#include "render/pipelines/entrypoints_internal.h"

bool main_pipelines_init(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                         SDL_GPUTextureFormat depth_format,
                         SDL_GPUTextureFormat swapchain_format)
{
    return main_pipelines_run_init(pipelines, device, color_format, depth_format, swapchain_format);
}

void main_pipelines_destroy(main_pipelines_t* pipelines, SDL_GPUDevice* device)
{
    main_pipelines_run_destroy(pipelines, device);
}
