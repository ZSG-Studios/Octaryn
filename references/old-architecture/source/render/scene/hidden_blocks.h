#pragma once

#include <SDL3/SDL.h>

#include "render/scene/passes.h"

struct main_render_hidden_block_uniforms_t
{
    Uint32 count;
    int _pad[3];
    int blocks[MAIN_RENDER_HIDDEN_BLOCK_CAPACITY][4];
};

inline void main_render_fill_hidden_block_uniforms(const main_render_pass_context_t* context,
                                                   main_render_hidden_block_uniforms_t* uniforms)
{
    SDL_zerop(uniforms);
    uniforms->count = context->hidden_block_count;
    for (Uint32 i = 0; i < context->hidden_block_count && i < MAIN_RENDER_HIDDEN_BLOCK_CAPACITY; ++i)
    {
        uniforms->blocks[i][0] = context->hidden_blocks[i][0];
        uniforms->blocks[i][1] = context->hidden_blocks[i][1];
        uniforms->blocks[i][2] = context->hidden_blocks[i][2];
        uniforms->blocks[i][3] = 0;
    }
}
