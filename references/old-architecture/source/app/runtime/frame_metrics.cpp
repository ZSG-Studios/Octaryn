#include "app/runtime/frame_metrics.h"

namespace {

constexpr float kWarmupSeconds = 5.0f;
constexpr Uint64 kWarmupNs = static_cast<Uint64>(kWarmupSeconds * 1000000000.0f);
constexpr Uint64 kCurrentWindowNs = 500000000ull;
constexpr float kBucketMs = 0.25f;
constexpr Uint32 kHistogramBins = 1024u;

float ms_to_fps(float value_ms)
{
    return value_ms > 0.0f ? 1000.0f / value_ms : 0.0f;
}

app_frame_metric_pair_t make_pair(float value_ms)
{
    return {
        .ms = value_ms,
        .fps = ms_to_fps(value_ms),
    };
}

Uint32 bucket_for_ms(float value_ms)
{
    if (value_ms <= 0.0f)
    {
        return 0u;
    }
    const Uint32 bucket = static_cast<Uint32>(value_ms / kBucketMs);
    return SDL_min(bucket, kHistogramBins - 1u);
}

float bucket_high_ms(Uint32 bucket)
{
    return static_cast<float>(bucket + 1u) * kBucketMs;
}

float percentile_ms(const app_frame_metrics_t* metrics, float percentile)
{
    if (!metrics || metrics->sample_count == 0u)
    {
        return 0.0f;
    }

    const double clamped = static_cast<double>(SDL_clamp(percentile, 0.0f, 1.0f));
    const Uint64 target = static_cast<Uint64>(clamped * static_cast<double>(metrics->sample_count - 1u)) + 1u;
    Uint64 seen = 0u;
    for (Uint32 bucket = 0; bucket < kHistogramBins; ++bucket)
    {
        seen += metrics->histogram[bucket];
        if (seen >= target)
        {
            return bucket_high_ms(bucket);
        }
    }
    return metrics->worst_ms;
}

app_frame_metric_pair_t confirmed_low(const app_frame_metrics_t* metrics, Uint32 required_hits, Uint32* out_hits)
{
    if (out_hits)
    {
        *out_hits = 0u;
    }
    if (!metrics || metrics->sample_count == 0u)
    {
        return {};
    }

    for (Sint32 bucket = static_cast<Sint32>(kHistogramBins) - 1; bucket >= 0; --bucket)
    {
        const Uint32 hits = metrics->histogram[static_cast<Uint32>(bucket)];
        if (hits >= required_hits)
        {
            if (out_hits)
            {
                *out_hits = hits;
            }
            return make_pair(bucket_high_ms(static_cast<Uint32>(bucket)));
        }
    }
    return {};
}

bool warmup_finished(const app_frame_metrics_t* metrics, Uint64 now_ticks)
{
    return metrics && metrics->first_sample_ticks != 0u && now_ticks >= metrics->first_sample_ticks + kWarmupNs;
}

float warmup_elapsed_seconds(const app_frame_metrics_t* metrics, Uint64 now_ticks)
{
    if (!metrics || metrics->first_sample_ticks == 0u || now_ticks <= metrics->first_sample_ticks)
    {
        return 0.0f;
    }
    return SDL_min(static_cast<float>(now_ticks - metrics->first_sample_ticks) * 1e-9f, kWarmupSeconds);
}

} // namespace

void app_frame_metrics_init(app_frame_metrics_t* metrics)
{
    if (!metrics)
    {
        return;
    }
    *metrics = {};
}

void app_frame_metrics_record(app_frame_metrics_t* metrics, float frame_ms, Uint64 sample_ticks)
{
    if (!metrics || frame_ms <= 0.0f || sample_ticks == 0u)
    {
        return;
    }
    if (metrics->first_sample_ticks == 0u)
    {
        metrics->first_sample_ticks = sample_ticks;
        metrics->current_window_start_ticks = sample_ticks;
    }
    metrics->last_sample_ticks = sample_ticks;

    if (sample_ticks - metrics->current_window_start_ticks >= kCurrentWindowNs)
    {
        if (metrics->current_window_sample_count > 0u)
        {
            metrics->current = make_pair(static_cast<float>(
                metrics->current_window_total_ms / static_cast<double>(metrics->current_window_sample_count)));
        }
        metrics->current_window_start_ticks = sample_ticks;
        metrics->current_window_sample_count = 0u;
        metrics->current_window_total_ms = 0.0;
    }

    metrics->current_window_total_ms += static_cast<double>(frame_ms);
    ++metrics->current_window_sample_count;

    if (!warmup_finished(metrics, sample_ticks))
    {
        return;
    }

    ++metrics->sample_count;
    metrics->total_ms += static_cast<double>(frame_ms);
    metrics->worst_ms = SDL_max(metrics->worst_ms, frame_ms);
    ++metrics->histogram[bucket_for_ms(frame_ms)];
}

app_frame_metrics_snapshot_t app_frame_metrics_snapshot(const app_frame_metrics_t* metrics, Uint64 now_ticks)
{
    if (!metrics)
    {
        return {};
    }
    if (metrics->first_sample_ticks != 0u &&
        (now_ticks == 0u || now_ticks < metrics->first_sample_ticks))
    {
        now_ticks = metrics->last_sample_ticks;
    }

    app_frame_metrics_snapshot_t snapshot = {};
    snapshot.current = metrics->current;
    snapshot.warmup_seconds = kWarmupSeconds;
    snapshot.warmup_elapsed_seconds = warmup_elapsed_seconds(metrics, now_ticks);
    snapshot.warmup_complete = warmup_finished(metrics, now_ticks);
    snapshot.sample_count = metrics->sample_count;

    if (metrics->current_window_sample_count > 0u)
    {
        snapshot.current = make_pair(static_cast<float>(
            metrics->current_window_total_ms / static_cast<double>(metrics->current_window_sample_count)));
    }
    if (metrics->sample_count == 0u)
    {
        return snapshot;
    }

    snapshot.average = make_pair(static_cast<float>(metrics->total_ms / static_cast<double>(metrics->sample_count)));
    snapshot.low_1pct = make_pair(percentile_ms(metrics, 0.99f));
    snapshot.low_0_1pct = make_pair(percentile_ms(metrics, 0.999f));
    snapshot.worst = make_pair(metrics->worst_ms);
    snapshot.confirmed_low_5 = confirmed_low(metrics, 5u, &snapshot.confirmed_low_5_hits);
    snapshot.confirmed_low_10 = confirmed_low(metrics, 10u, &snapshot.confirmed_low_10_hits);
    return snapshot;
}
