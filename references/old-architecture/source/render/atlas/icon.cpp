#include "render/atlas/internal.h"

#include "render/atlas/texture.h"

void main_render_atlas_set_window_icon_internal(const main_render_resources_t* resources,
                                                SDL_Window* window,
                                                block_t block)
{
    if (!resources->atlas_surface)
    {
        return;
    }

    SDL_Surface* icon = SDL_CreateSurface(MAIN_RENDER_ATLAS_BLOCK_WIDTH, MAIN_RENDER_ATLAS_BLOCK_WIDTH,
                                          SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return;
    }

    SDL_Rect src = {};
    src.x = block_get_index(block, DIRECTION_NORTH) * MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    src.w = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    src.h = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    SDL_Rect dst = {};
    dst.w = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    dst.h = MAIN_RENDER_ATLAS_BLOCK_WIDTH;
    if (!SDL_BlitSurface(resources->atlas_surface, &src, icon, &dst))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return;
    }

    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}
