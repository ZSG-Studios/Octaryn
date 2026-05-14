#include "app/runtime/render/passes.h"

#include "app/runtime/diagnostics.h"
#include "core/log.h"

#include <SDL3/SDL.h>

namespace {

bool env_flag_enabled(const char* name)
{
    const char* value = SDL_getenv(name);
    return value && value[0] != '\0' && SDL_strcmp(value, "0") != 0 && SDL_strcasecmp(value, "false") != 0 &&
        SDL_strcasecmp(value, "off") != 0;
}

bool should_log_scene_pass_startup_diagnostics()
{
    if (!env_flag_enabled("OCTARYN_LOG_SCENE_PASSES"))
    {
        return false;
    }

    static bool logged_scene_passes = false;
    if (logged_scene_passes)
    {
        return false;
    }

    logged_scene_passes = true;
    return true;
}

void log_scene_pass_diagnostic(bool enabled, const char* message)
{
    if (enabled)
    {
        oct_log_infof("%s", message);
    }
}

} // namespace

void app_execute_scene_passes(SDL_GPUCommandBuffer* cbuf,
                              main_render_pass_context_t* render_context,
                              main_frame_profile_sample_t* profile_sample)
{
    const bool log_pass_diagnostics = should_log_scene_pass_startup_diagnostics();

    if (profile_sample)
    {
        profile_sample->depth_ms = 0.0f;
    }

    Uint64 stage_start = app_profile_now();
    log_scene_pass_diagnostic(log_pass_diagnostics, "passes: gbuffer");
    main_render_pass_gbuffer(cbuf, render_context, profile_sample);
    log_scene_pass_diagnostic(log_pass_diagnostics, "passes: after gbuffer");
    if (profile_sample)
    {
        profile_sample->gbuffer_ms = app_profile_elapsed_ms(stage_start);
    }

    stage_start = app_profile_now();
    log_scene_pass_diagnostic(log_pass_diagnostics, "passes: composite");
    main_render_pass_composite(cbuf, render_context);
    log_scene_pass_diagnostic(log_pass_diagnostics, "passes: after composite");
    if (profile_sample)
    {
        profile_sample->composite_ms = app_profile_elapsed_ms(stage_start);
        profile_sample->post_ms = profile_sample->composite_ms + profile_sample->depth_ms;
    }

    stage_start = app_profile_now();
    main_render_pass_forward(cbuf, render_context);
    if (profile_sample)
    {
        profile_sample->forward_ms = app_profile_elapsed_ms(stage_start);
    }
}
