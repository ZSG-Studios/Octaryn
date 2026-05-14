#include "app/runtime/render/scene.h"

#include "app/runtime/internal.h"
#include "app/runtime/render/context.h"
#include "app/runtime/render/passes.h"

void app_render_scene(SDL_GPUCommandBuffer* cbuf, main_frame_profile_sample_t* profile_sample)
{
    main_render_pass_context_t render_context = {};
    app_build_render_context(&render_context);
    app_execute_scene_passes(cbuf, &render_context, profile_sample);
}
