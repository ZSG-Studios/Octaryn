#include "render/atlas/internal.h"

void main_render_atlas_destroy_internal(main_render_resources_t* resources, SDL_GPUDevice* device)
{
    if (resources->atlas_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->atlas_texture);
        resources->atlas_texture = nullptr;
    }
    if (resources->atlas_normal_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->atlas_normal_texture);
        resources->atlas_normal_texture = nullptr;
    }
    if (resources->atlas_specular_texture)
    {
        SDL_ReleaseGPUTexture(device, resources->atlas_specular_texture);
        resources->atlas_specular_texture = nullptr;
    }
    if (resources->atlas_sampler)
    {
        SDL_ReleaseGPUSampler(device, resources->atlas_sampler);
        resources->atlas_sampler = nullptr;
    }
    if (resources->cutout_sampler)
    {
        SDL_ReleaseGPUSampler(device, resources->cutout_sampler);
        resources->cutout_sampler = nullptr;
    }
    if (resources->nearest_sampler)
    {
        SDL_ReleaseGPUSampler(device, resources->nearest_sampler);
        resources->nearest_sampler = nullptr;
    }
    if (resources->atlas_surface)
    {
        SDL_DestroySurface(resources->atlas_surface);
        resources->atlas_surface = nullptr;
    }
    if (resources->atlas_normal_surface)
    {
        SDL_DestroySurface(resources->atlas_normal_surface);
        resources->atlas_normal_surface = nullptr;
    }
    if (resources->atlas_specular_surface)
    {
        SDL_DestroySurface(resources->atlas_specular_surface);
        resources->atlas_specular_surface = nullptr;
    }
    if (resources->atlas_animation_surface)
    {
        SDL_DestroySurface(resources->atlas_animation_surface);
        resources->atlas_animation_surface = nullptr;
    }
}
