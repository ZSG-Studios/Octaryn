#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_METRICS_HISTOGRAM_BINS 1024u

typedef struct frame_metric_pair
{
    float ms;
    float fps;
} frame_metric_pair;

typedef struct frame_metrics_snapshot
{
    frame_metric_pair current;
    frame_metric_pair average;
    frame_metric_pair low_1pct;
    frame_metric_pair low_0_1pct;
    frame_metric_pair confirmed_low_5;
    frame_metric_pair confirmed_low_10;
    frame_metric_pair worst;
    float warmup_elapsed_seconds;
    float warmup_seconds;
    uint64_t sample_count;
    uint32_t confirmed_low_5_hits;
    uint32_t confirmed_low_10_hits;
    uint8_t warmup_complete;
} frame_metrics_snapshot;

typedef struct frame_metrics
{
    uint64_t first_sample_ticks;
    uint64_t last_sample_ticks;
    uint64_t current_window_start_ticks;
    uint64_t current_window_sample_count;
    double current_window_total_ms;
    frame_metric_pair current;
    uint64_t sample_count;
    double total_ms;
    float worst_ms;
    uint32_t histogram[FRAME_METRICS_HISTOGRAM_BINS];
} frame_metrics;

void frame_metrics_init(frame_metrics* metrics);
void frame_metrics_record(
    frame_metrics* metrics,
    float frame_ms,
    uint64_t sample_ticks);
frame_metrics_snapshot frame_metrics_snapshot_value(
    const frame_metrics* metrics,
    uint64_t now_ticks);

#ifdef __cplusplus
}
#endif
