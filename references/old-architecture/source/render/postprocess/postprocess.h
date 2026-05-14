#pragma once

#include "render/scene/passes.h"

void main_render_pass_composite(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context);
void main_render_pass_depth(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context);
