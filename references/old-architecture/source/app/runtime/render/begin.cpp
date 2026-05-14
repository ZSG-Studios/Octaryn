#include "app/runtime/render/begin.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/render/context.h"
#include "core/profile.h"

bool app_begin_render_commands(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample)
{
    OCT_PROFILE_ZONE("render.acquire_command_buffer");
    if (!frame)
    {
        return false;
    }

    const Uint64 stage_start = app_profile_now();
    frame->cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (profile_sample)
    {
        profile_sample->command_acquire_ms = app_profile_elapsed_ms(stage_start);
    }
    if (!frame->cbuf)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool app_acquire_render_swapchain(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample)
{
    OCT_PROFILE_ZONE("render.acquire_swapchain");
    if (!frame || !frame->cbuf)
    {
        return false;
    }

    frame->swapchain_unavailable = false;
    const Uint64 begin_start = app_profile_now();
    Uint64 stage_start = begin_start;
    const bool nonblocking = frame_pacing.acquire_mode != APP_SWAPCHAIN_ACQUIRE_EARLY;
    bool acquired = false;
    stage_start = app_profile_now();
    if (nonblocking)
    {
        acquired = SDL_AcquireGPUSwapchainTexture(frame->cbuf,
                                                 window,
                                                 &frame->swapchain_texture,
                                                 &frame->width,
                                                 &frame->height);
        if (profile_sample)
        {
            profile_sample->swapchain_acquire_ms = app_profile_elapsed_ms(stage_start);
        }
    }
    else
    {
        acquired = SDL_WaitAndAcquireGPUSwapchainTexture(frame->cbuf,
                                                        window,
                                                        &frame->swapchain_texture,
                                                        &frame->width,
                                                        &frame->height);
        if (profile_sample)
        {
            profile_sample->swapchain_wait_ms = app_profile_elapsed_ms(stage_start);
        }
    }
    if (!acquired)
    {
        if (profile_sample)
        {
            profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
        }
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(frame->cbuf);
        return false;
    }

    if (!frame->swapchain_texture || !frame->width || !frame->height)
    {
        frame->swapchain_unavailable = true;
        if (profile_sample)
        {
            profile_sample->swapchain_skipped = 1u;
        }
        SDL_CancelGPUCommandBuffer(frame->cbuf);
        const float skip_sleep_ms = app_frame_pacing_sleep_after_swapchain_unavailable(&frame_pacing);
        if (profile_sample)
        {
            profile_sample->fps_cap_sleep_ms += skip_sleep_ms;
            profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
        }
        return false;
    }
    if (profile_sample)
    {
        profile_sample->swapchain_acquired = 1u;
    }

    const int swapchain_width = (int) frame->width;
    const int swapchain_height = (int) frame->height;
    if (static_cast<Uint32>(swapchain_width) != resources.window_width ||
        static_cast<Uint32>(swapchain_height) != resources.window_height)
    {
        if (!frame->resize_allowed)
        {
            if (profile_sample)
            {
                profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
            }
            return true;
        }

        stage_start = app_profile_now();
        const bool resized = app_resize_render_targets(swapchain_width, swapchain_height);
        if (profile_sample)
        {
            profile_sample->resize_ms = app_profile_elapsed_ms(stage_start);
        }
        if (!resized)
        {
            SDL_SubmitGPUCommandBuffer(frame->cbuf);
            if (profile_sample)
            {
                profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
            }
            return false;
        }
    }

    if (profile_sample)
    {
        profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
    }
    return true;
}

bool app_begin_render_frame(app_render_begin_t* frame, main_frame_profile_sample_t* profile_sample)
{
    const Uint64 begin_start = app_profile_now();
    if (!app_begin_render_commands(frame, profile_sample))
    {
        if (profile_sample)
        {
            profile_sample->frame_acquire_ms = app_profile_elapsed_ms(begin_start);
        }
        return false;
    }

    if (!app_acquire_render_swapchain(frame, profile_sample))
    {
        return false;
    }
    return true;
}
