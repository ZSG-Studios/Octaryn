#include "FrameStatistics.h"

namespace {

constexpr uint64_t kNanosecondsPerSecond = 1000000000ull;
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ull;
constexpr uint64_t kSubmittedFpsWindowNs = kNanosecondsPerSecond / 4ull;
constexpr uint64_t kSubmittedFpsStaleNs = kNanosecondsPerSecond;

auto rounded_uint32(double value) -> uint32_t
{
    if (value <= 0.0)
    {
        return 0u;
    }

    return static_cast<uint32_t>(value + 0.5);
}

void set_display_fps(window_frame_statistics* statistics, float fps, float frame_ms)
{
    statistics->display_fps_tenths = rounded_uint32(static_cast<double>(fps) * 10.0);
    statistics->display_frame_time_hundredths = rounded_uint32(static_cast<double>(frame_ms) * 100.0);
}

} // namespace

void window_frame_statistics_init(window_frame_statistics* statistics)
{
    if (statistics == nullptr)
    {
        return;
    }

    *statistics = {};
}

void window_frame_statistics_note_submitted_frame(
    window_frame_statistics* statistics,
    uint64_t now_ticks)
{
    if (statistics == nullptr)
    {
        return;
    }

    if (statistics->submitted_fps_window_ticks == 0u)
    {
        statistics->submitted_fps_window_ticks = now_ticks;
        statistics->submitted_fps_window_frames = 0u;
    }
    if (statistics->last_submitted_frame_ticks != 0u && now_ticks > statistics->last_submitted_frame_ticks)
    {
        const double frame_ms = static_cast<double>(now_ticks - statistics->last_submitted_frame_ticks) /
            static_cast<double>(kNanosecondsPerMillisecond);
        const double fps = frame_ms > 0.0 ? 1000.0 / frame_ms : 0.0;
        statistics->last_submitted_fps_tenths = rounded_uint32(fps * 10.0);
        statistics->last_submitted_frame_time_hundredths = rounded_uint32(frame_ms * 100.0);
    }
    statistics->last_submitted_frame_ticks = now_ticks;
    ++statistics->submitted_fps_window_frames;

    const uint64_t elapsed_ticks = now_ticks - statistics->submitted_fps_window_ticks;
    if (elapsed_ticks < kSubmittedFpsWindowNs)
    {
        return;
    }

    const double elapsed_seconds = static_cast<double>(elapsed_ticks) / static_cast<double>(kNanosecondsPerSecond);
    const double fps = elapsed_seconds > 0.0 ?
        static_cast<double>(statistics->submitted_fps_window_frames) / elapsed_seconds :
        0.0;
    const double frame_ms = fps > 0.0 ? 1000.0 / fps : 0.0;
    statistics->submitted_fps_tenths = rounded_uint32(fps * 10.0);
    statistics->submitted_frame_time_hundredths = rounded_uint32(frame_ms * 100.0);
    statistics->submitted_fps_window_ticks = now_ticks;
    statistics->submitted_fps_window_frames = 0u;
}

void window_frame_statistics_update_display(
    window_frame_statistics* statistics,
    uint64_t now_ticks)
{
    if (statistics == nullptr)
    {
        return;
    }

    const bool submitted_fps_fresh = statistics->submitted_fps_window_ticks != 0u &&
        now_ticks - statistics->submitted_fps_window_ticks <= kSubmittedFpsStaleNs &&
        statistics->submitted_fps_tenths != 0u;
    const bool submitted_fps_known = statistics->submitted_fps_window_ticks != 0u;
    float display_fps = 0.0f;
    float display_ms = 0.0f;
    if (submitted_fps_fresh)
    {
        display_fps = static_cast<float>(statistics->submitted_fps_tenths) * 0.1f;
        display_ms = static_cast<float>(statistics->submitted_frame_time_hundredths) * 0.01f;
    }
    else if (submitted_fps_known)
    {
        display_fps = 0.0f;
        display_ms = 0.0f;
    }
    set_display_fps(statistics, display_fps, display_ms);
}

uint32_t window_frame_statistics_display_fps_tenths(
    const window_frame_statistics* statistics)
{
    return statistics != nullptr ? statistics->display_fps_tenths : 0u;
}

uint32_t window_frame_statistics_display_frame_time_hundredths(
    const window_frame_statistics* statistics)
{
    return statistics != nullptr ? statistics->display_frame_time_hundredths : 0u;
}

uint32_t window_frame_statistics_submitted_fps_tenths(
    const window_frame_statistics* statistics)
{
    return statistics != nullptr ? statistics->submitted_fps_tenths : 0u;
}

uint32_t window_frame_statistics_submitted_frame_time_hundredths(
    const window_frame_statistics* statistics)
{
    return statistics != nullptr ? statistics->submitted_frame_time_hundredths : 0u;
}

uint32_t window_frame_statistics_last_submitted_fps_tenths(
    const window_frame_statistics* statistics)
{
    if (statistics == nullptr)
    {
        return 0u;
    }

    return statistics->last_submitted_fps_tenths != 0u ?
        statistics->last_submitted_fps_tenths :
        window_frame_statistics_submitted_fps_tenths(statistics);
}

uint32_t window_frame_statistics_last_submitted_frame_time_hundredths(
    const window_frame_statistics* statistics)
{
    if (statistics == nullptr)
    {
        return 0u;
    }

    return statistics->last_submitted_frame_time_hundredths != 0u ?
        statistics->last_submitted_frame_time_hundredths :
        window_frame_statistics_submitted_frame_time_hundredths(statistics);
}
