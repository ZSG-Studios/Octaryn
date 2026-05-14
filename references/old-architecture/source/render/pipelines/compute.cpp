#include "render/pipelines/internal.h"

#include "render/shader/shader.h"

bool main_pipelines_load_compute(SDL_GPUComputePipeline** pipeline, SDL_GPUDevice* device, const char* path)
{
    *pipeline = static_cast<SDL_GPUComputePipeline*>(shader_load(device, path));
    return *pipeline != NULL;
}
