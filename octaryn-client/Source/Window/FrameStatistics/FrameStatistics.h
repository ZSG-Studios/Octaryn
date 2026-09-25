#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct window_frame_statistics
{
    uint64_t submitted_fps_window_ticks;
    uint64_t last_submitted_frame_ticks;
    uint32_t submitted_fps_window_frames;
    uint32_t display_fps_tenths;
    uint32_t display_frame_time_hundredths;
    uint32_t submitted_fps_tenths;
    uint32_t submitted_frame_time_hundredths;
    uint32_t last_submitted_fps_tenths;
    uint32_t last_submitted_frame_time_hundredths;
} window_frame_statistics;

void window_frame_statistics_init(window_frame_statistics* statistics);
void window_frame_statistics_note_submitted_frame(
    window_frame_statistics* statistics,
    uint64_t now_ticks);
void window_frame_statistics_update_display(
    window_frame_statistics* statistics,
    uint64_t now_ticks);
uint32_t window_frame_statistics_display_fps_tenths(
    const window_frame_statistics* statistics);
uint32_t window_frame_statistics_display_frame_time_hundredths(
    const window_frame_statistics* statistics);
uint32_t window_frame_statistics_submitted_fps_tenths(
    const window_frame_statistics* statistics);
uint32_t window_frame_statistics_submitted_frame_time_hundredths(
    const window_frame_statistics* statistics);
uint32_t window_frame_statistics_last_submitted_fps_tenths(
    const window_frame_statistics* statistics);
uint32_t window_frame_statistics_last_submitted_frame_time_hundredths(
    const window_frame_statistics* statistics);

#ifdef __cplusplus
}
#endif
