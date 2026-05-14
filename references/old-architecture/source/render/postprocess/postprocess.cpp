#include "render/postprocess/postprocess.h"

#include "render/postprocess/composite.h"

void main_render_pass_composite(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    main_render_postprocess_composite(cbuf, context);
}

void main_render_pass_depth(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    main_render_postprocess_depth(cbuf, context);
}
