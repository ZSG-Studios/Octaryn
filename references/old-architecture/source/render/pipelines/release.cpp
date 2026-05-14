#include "render/pipelines/lifecycle_internal.h"

void main_pipelines_release_compute(main_pipelines_t* pipelines, SDL_GPUDevice* device)
{
    if (pipelines->composite)
    {
        SDL_ReleaseGPUComputePipeline(device, pipelines->composite);
    }
    if (pipelines->ui)
    {
        SDL_ReleaseGPUComputePipeline(device, pipelines->ui);
    }
}

void main_pipelines_release_graphics(main_pipelines_t* pipelines, SDL_GPUDevice* device)
{
    if (pipelines->selection)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->selection);
    }
    if (pipelines->present)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->present);
    }
    if (pipelines->sky)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->sky);
    }
    if (pipelines->clouds)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->clouds);
    }
    if (pipelines->depth)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->depth);
    }
    if (pipelines->transparent)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->transparent);
    }
    if (pipelines->water)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->water);
    }
    if (pipelines->lava)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->lava);
    }
    if (pipelines->opaque)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->opaque);
    }
    if (pipelines->opaque_sprite)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines->opaque_sprite);
    }
}
