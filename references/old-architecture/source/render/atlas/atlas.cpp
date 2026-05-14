#include "render/atlas/atlas.h"

#include "render/atlas/internal.h"
#include "render/atlas/texture.h"

bool main_render_atlas_init(main_render_resources_t* resources, SDL_GPUDevice* device)
{
    return main_render_atlas_create_texture(resources, device)
        && main_render_atlas_create_samplers(resources, device)
        && main_render_atlas_load_animations_internal(resources);
}

void main_render_atlas_update_animations(main_render_resources_t* resources, SDL_GPUDevice* device, Uint64 ticks_ns)
{
    main_render_atlas_update_animations_internal(resources, device, ticks_ns);
}

void main_render_atlas_set_window_icon(const main_render_resources_t* resources, SDL_Window* window, block_t block)
{
    main_render_atlas_set_window_icon_internal(resources, window, block);
}

void main_render_atlas_destroy(main_render_resources_t* resources, SDL_GPUDevice* device)
{
    main_render_atlas_destroy_internal(resources, device);
}
