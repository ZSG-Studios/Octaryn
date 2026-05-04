#include "octaryn_client_frame_profile.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>

float octaryn_client_frame_profile_elapsed_ms(uint64_t start_ticks, uint64_t end_ticks)
{
    return end_ticks >= start_ticks
        ? static_cast<float>(end_ticks - start_ticks) * 1.0e-6f
        : 0.0f;
}

float octaryn_client_frame_profile_elapsed_ms_since(uint64_t start_ticks)
{
    return octaryn_client_frame_profile_elapsed_ms(start_ticks, SDL_GetTicksNS());
}

float octaryn_client_frame_profile_fps_from_ms(float value_ms)
{
    return value_ms > 0.0f ? 1000.0f / value_ms : 0.0f;
}

uint32_t octaryn_client_frame_profile_hundredths_from_ms(float value_ms)
{
    if (value_ms <= 0.0f)
    {
        return 0u;
    }

    const double scaled = static_cast<double>(value_ms) * 100.0;
    return scaled > static_cast<double>(UINT32_MAX)
        ? UINT32_MAX
        : static_cast<uint32_t>(scaled + 0.5);
}

uint32_t octaryn_client_frame_profile_hundredths_from_seconds(float value_seconds)
{
    if (value_seconds <= 0.0f)
    {
        return 0u;
    }

    const double scaled = static_cast<double>(value_seconds) * 100.0;
    return scaled > static_cast<double>(UINT32_MAX)
        ? UINT32_MAX
        : static_cast<uint32_t>(scaled + 0.5);
}

uint32_t octaryn_client_frame_profile_tenths_from_fps(float value_fps)
{
    if (value_fps <= 0.0f)
    {
        return 0u;
    }

    const double scaled = static_cast<double>(value_fps) * 10.0;
    return scaled > static_cast<double>(UINT32_MAX)
        ? UINT32_MAX
        : static_cast<uint32_t>(scaled + 0.5);
}

void octaryn_client_frame_profile_finalize_sample(octaryn_client_frame_profile_sample* sample)
{
    if (sample == nullptr)
    {
        return;
    }

    sample->gbuffer_ms = sample->gbuffer_sky_ms + sample->gbuffer_opaque_ms + sample->gbuffer_sprite_ms;
    sample->post_ms =
        sample->composite_ms +
        sample->depth_ms +
        sample->forward_ms +
        sample->ui_ms +
        sample->imgui_ms +
        sample->swapchain_blit_ms;
    const float known_render_ms =
        sample->render_setup_ms +
        sample->gbuffer_ms +
        sample->post_ms +
        sample->render_submit_ms;
    sample->render_other_ms = std::max(0.0f, sample->render_ms - known_render_ms);
    sample->accounted_ms =
        sample->sim_ms +
        sample->misc_ms +
        sample->world_ms +
        sample->render_ms +
        sample->fps_cap_sleep_ms;
    sample->submitted_accounted_ms = sample->accounted_ms;
    sample->untracked_ms = std::max(0.0f, sample->total_ms - sample->accounted_ms);
}
