#include "app/runtime/render/render.h"

#include "app/overlay/interaction.h"
#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/render/begin.h"
#include "app/runtime/render/context.h"
#include "app/runtime/render/scene.h"
#include "app/runtime/render/submit.h"

#include "core/profile.h"
#include "render/atlas/atlas.h"

namespace {

constexpr Uint64 kLightingTuningPersistDebounceNs = SDL_NS_PER_SECOND / 2ull;

bool sync_deferred_render_target_size(main_frame_profile_sample_t* profile_sample)
{
    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &window_width, &window_height))
    {
        return true;
    }
    if (window_width <= 0 || window_height <= 0 ||
        (static_cast<Uint32>(window_width) == resources.window_width &&
         static_cast<Uint32>(window_height) == resources.window_height))
    {
        return true;
    }

    const Uint64 resize_start = app_profile_now();
    const bool resized = app_resize_render_targets(window_width, window_height);
    if (profile_sample)
    {
        profile_sample->resize_ms += app_profile_elapsed_ms(resize_start);
    }
    return resized;
}

} // namespace

void app_render(main_frame_profile_sample_t* profile_sample)
{
    OCT_PROFILE_ZONE("render");
    if (profile_sample)
    {
        profile_sample->render_attempted = 1u;
    }

    app_render_begin_t frame = {};
    const bool defer_swapchain_acquire = app_frame_pacing_should_defer_swapchain_acquire(&frame_pacing);
    const bool probe_swapchain_before_scene =
        app_frame_pacing_should_probe_swapchain_before_scene(&frame_pacing);
    if (defer_swapchain_acquire)
    {
        if (!app_begin_render_commands(&frame, profile_sample))
        {
            oct_log_warnf("app_render: app_begin_render_commands returned false");
            return;
        }
        if (probe_swapchain_before_scene)
        {
            if (!app_acquire_render_swapchain(&frame, profile_sample))
            {
                if (!frame.swapchain_unavailable)
                {
                    oct_log_warnf("app_render: app_acquire_render_swapchain returned false");
                }
                return;
            }
        }
        else if (!sync_deferred_render_target_size(profile_sample))
        {
            oct_log_warnf("app_render: sync_deferred_render_target_size returned false");
            SDL_CancelGPUCommandBuffer(frame.cbuf);
            return;
        }
        if (!probe_swapchain_before_scene)
        {
            frame.resize_allowed = false;
        }
    }
    else
    {
        if (!app_begin_render_commands(&frame, profile_sample))
        {
            oct_log_warnf("app_render: app_begin_render_commands returned false");
            return;
        }
        if (!app_acquire_render_swapchain(&frame, profile_sample))
        {
            if (!frame.swapchain_unavailable)
            {
                oct_log_warnf("app_render: app_acquire_render_swapchain returned false");
            }
            return;
        }
    }

    const Uint64 render_start = app_profile_now();
    Uint64 stage_start = render_start;
    if (main_imgui_lighting_begin_frame(&lighting_tuning))
    {
        lighting_tuning_dirty = true;
        lighting_tuning_change_ticks = ticks2;
    }
    if (lighting_tuning_dirty &&
        ticks2 - lighting_tuning_change_ticks >= kLightingTuningPersistDebounceNs)
    {
        main_lighting_settings_save(&lighting_tuning);
        lighting_tuning_dirty = false;
    }
    app_sync_relative_mouse_mode_for_ui();
    main_render_atlas_update_animations(&resources, device, ticks2);
    camera_update(&player.camera);
    if (profile_sample)
    {
        profile_sample->render_setup_ms = app_profile_elapsed_ms(stage_start);
    }

    app_render_scene(frame.cbuf, profile_sample);

    if (defer_swapchain_acquire &&
        !probe_swapchain_before_scene &&
        !app_acquire_render_swapchain(&frame, profile_sample))
    {
        if (!frame.swapchain_unavailable)
        {
            oct_log_warnf("app_render: app_acquire_render_swapchain returned false");
        }
        return;
    }

    app_submit_render_frame(frame.cbuf, frame.swapchain_texture, profile_sample);

    if (profile_sample)
    {
        float render_elapsed_ms = app_profile_elapsed_ms(render_start);
        if (defer_swapchain_acquire && !probe_swapchain_before_scene)
        {
            render_elapsed_ms = SDL_max(0.0f, render_elapsed_ms - profile_sample->frame_acquire_ms);
        }
        profile_sample->render_ms = render_elapsed_ms;
    }
}
