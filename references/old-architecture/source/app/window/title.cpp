#include "internal.h"

namespace {

constexpr Uint64 kTitleUpdateNs = SDL_NS_PER_SECOND / 4ull;
constexpr Uint64 kSubmittedFpsWindowNs = SDL_NS_PER_SECOND / 4ull;
constexpr Uint64 kSubmittedFpsStaleNs = SDL_NS_PER_SECOND;

void set_display_fps(main_window_t* state, float fps, float frame_ms)
{
    state->debug_fps_tenths = static_cast<Uint32>(fps * 10.0f + 0.5f);
    state->debug_frame_time_hundredths = static_cast<Uint32>(frame_ms * 100.0f + 0.5f);
}

} // namespace

void main_window_update_title(main_window_t* state, SDL_Window* window, const char* app_name, Uint64 now_ticks, float dt)
{
    if (dt <= SDL_FLT_EPSILON)
    {
        return;
    }
    state->accumulated_ms += dt;
    state->accumulated_frames++;
    const float average_ms = state->accumulated_ms / static_cast<float>(state->accumulated_frames);
    const float fps = 1000.0f / average_ms;
    state->debug_loop_fps_tenths = static_cast<Uint32>(fps * 10.0f + 0.5f);
    state->debug_loop_frame_time_hundredths = static_cast<Uint32>(average_ms * 100.0f + 0.5f);

    const bool submitted_fps_fresh = state->submitted_fps_window_ticks != 0u &&
        now_ticks - state->submitted_fps_window_ticks <= kSubmittedFpsStaleNs &&
        state->debug_submitted_fps_tenths != 0u;
    const bool submitted_fps_known = state->submitted_fps_window_ticks != 0u;
    float display_fps = 0.0f;
    float display_ms = 0.0f;
    if (submitted_fps_fresh)
    {
        display_fps = static_cast<float>(state->debug_submitted_fps_tenths) * 0.1f;
        display_ms = static_cast<float>(state->debug_submitted_frame_time_hundredths) * 0.01f;
    }
    else if (submitted_fps_known)
    {
        display_fps = 0.0f;
        display_ms = 0.0f;
    }
    set_display_fps(state, display_fps, display_ms);

    if (state->last_title_update && now_ticks - state->last_title_update < kTitleUpdateNs)
    {
        return;
    }
    SDL_SetWindowTitle(window, app_name);
    state->accumulated_ms = 0.0f;
    state->accumulated_frames = 0;
    state->last_title_update = now_ticks;
}

void main_window_note_submitted_frame(main_window_t* state, Uint64 now_ticks)
{
    if (!state)
    {
        return;
    }
    if (state->submitted_fps_window_ticks == 0u)
    {
        state->submitted_fps_window_ticks = now_ticks;
        state->submitted_fps_window_frames = 0u;
    }
    if (state->last_submitted_frame_ticks != 0u && now_ticks > state->last_submitted_frame_ticks)
    {
        const double frame_ms = static_cast<double>(now_ticks - state->last_submitted_frame_ticks) /
            static_cast<double>(SDL_NS_PER_MS);
        const double fps = frame_ms > 0.0 ? 1000.0 / frame_ms : 0.0;
        state->debug_last_submitted_fps_tenths = static_cast<Uint32>(fps * 10.0 + 0.5);
        state->debug_last_submitted_frame_time_hundredths = static_cast<Uint32>(frame_ms * 100.0 + 0.5);
    }
    state->last_submitted_frame_ticks = now_ticks;
    state->submitted_fps_window_frames++;

    const Uint64 elapsed_ticks = now_ticks - state->submitted_fps_window_ticks;
    if (elapsed_ticks < kSubmittedFpsWindowNs)
    {
        return;
    }

    const double elapsed_seconds = static_cast<double>(elapsed_ticks) / static_cast<double>(SDL_NS_PER_SECOND);
    const double fps = elapsed_seconds > 0.0 ?
        static_cast<double>(state->submitted_fps_window_frames) / elapsed_seconds :
        0.0;
    const double frame_ms = fps > 0.0 ? 1000.0 / fps : 0.0;
    state->debug_submitted_fps_tenths = static_cast<Uint32>(fps * 10.0 + 0.5);
    state->debug_submitted_frame_time_hundredths = static_cast<Uint32>(frame_ms * 100.0 + 0.5);
    state->submitted_fps_window_ticks = now_ticks;
    state->submitted_fps_window_frames = 0u;
}

Uint32 main_window_debug_fps_tenths(const main_window_t* state)
{
    return state->debug_fps_tenths;
}

Uint32 main_window_debug_frame_time_hundredths(const main_window_t* state)
{
    return state->debug_frame_time_hundredths;
}

Uint32 main_window_debug_submitted_fps_tenths(const main_window_t* state)
{
    return state->debug_submitted_fps_tenths;
}

Uint32 main_window_debug_submitted_frame_time_hundredths(const main_window_t* state)
{
    return state->debug_submitted_frame_time_hundredths;
}

Uint32 main_window_debug_last_submitted_fps_tenths(const main_window_t* state)
{
    return state->debug_last_submitted_fps_tenths != 0u ?
        state->debug_last_submitted_fps_tenths :
        main_window_debug_submitted_fps_tenths(state);
}

Uint32 main_window_debug_last_submitted_frame_time_hundredths(const main_window_t* state)
{
    return state->debug_last_submitted_frame_time_hundredths != 0u ?
        state->debug_last_submitted_frame_time_hundredths :
        main_window_debug_submitted_frame_time_hundredths(state);
}
