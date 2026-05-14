#include "render/postprocess/composite_internal.h"

#include "render/scene/pass_shared.h"

main_render_composite_selection_t main_render_build_composite_selection_internal(const main_render_pass_context_t* context)
{
    return {
        {context->player->query.current[0], context->player->query.current[1], context->player->query.current[2]},
        main_render_get_query_hit_face(context->player),
    };
}
