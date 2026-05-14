#pragma once

#include "render/scene/passes.h"

struct main_render_composite_selection_t
{
    int block[3];
    Uint32 face;
};

main_render_composite_selection_t main_render_build_composite_selection_internal(
    const main_render_pass_context_t* context);
