#pragma once

#include <SDL3/SDL.h>

typedef struct app_frame_metric_pair
{
    float ms;
    float fps;
}
app_frame_metric_pair_t;

typedef struct app_frame_metrics_snapshot
{
    app_frame_metric_pair_t current;
    app_frame_metric_pair_t average;
    app_frame_metric_pair_t low_1pct;
    app_frame_metric_pair_t low_0_1pct;
    app_frame_metric_pair_t confirmed_low_5;
    app_frame_metric_pair_t confirmed_low_10;
    app_frame_metric_pair_t worst;
    float warmup_elapsed_seconds;
    float warmup_seconds;
    Uint64 sample_count;
    Uint32 confirmed_low_5_hits;
    Uint32 confirmed_low_10_hits;
    bool warmup_complete;
}
app_frame_metrics_snapshot_t;

typedef struct app_frame_metrics
{
    Uint64 first_sample_ticks;
    Uint64 last_sample_ticks;
    Uint64 current_window_start_ticks;
    Uint64 current_window_sample_count;
    double current_window_total_ms;
    app_frame_metric_pair_t current;
    Uint64 sample_count;
    double total_ms;
    float worst_ms;
    Uint32 histogram[1024];
}
app_frame_metrics_t;

void app_frame_metrics_init(app_frame_metrics_t* metrics);
void app_frame_metrics_record(app_frame_metrics_t* metrics, float frame_ms, Uint64 sample_ticks);
app_frame_metrics_snapshot_t app_frame_metrics_snapshot(const app_frame_metrics_t* metrics, Uint64 now_ticks);
