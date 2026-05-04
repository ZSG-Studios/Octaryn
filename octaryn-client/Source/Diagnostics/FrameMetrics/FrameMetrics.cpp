#include "FrameMetrics.h"

namespace {

constexpr float kWarmupSeconds = 5.0f;
constexpr uint64_t kWarmupNs = static_cast<uint64_t>(kWarmupSeconds * 1000000000.0f);
constexpr uint64_t kCurrentWindowNs = 500000000ull;
constexpr float kBucketMs = 0.25f;
constexpr uint32_t kHistogramBins = FRAME_METRICS_HISTOGRAM_BINS;

auto min_float(float left, float right) -> float
{
    return left < right ? left : right;
}

auto max_float(float left, float right) -> float
{
    return left > right ? left : right;
}

auto clamp_float(float value, float minimum, float maximum) -> float
{
    return max_float(minimum, min_float(value, maximum));
}

auto ms_to_fps(float value_ms) -> float
{
    return value_ms > 0.0f ? 1000.0f / value_ms : 0.0f;
}

auto make_pair(float value_ms) -> frame_metric_pair
{
    return {
        .ms = value_ms,
        .fps = ms_to_fps(value_ms),
    };
}

auto bucket_for_ms(float value_ms) -> uint32_t
{
    if (value_ms <= 0.0f)
    {
        return 0u;
    }

    const uint32_t bucket = static_cast<uint32_t>(value_ms / kBucketMs);
    return bucket < kHistogramBins ? bucket : kHistogramBins - 1u;
}

auto bucket_high_ms(uint32_t bucket) -> float
{
    return static_cast<float>(bucket + 1u) * kBucketMs;
}

auto percentile_ms(const frame_metrics* metrics, float percentile) -> float
{
    if (metrics == nullptr || metrics->sample_count == 0u)
    {
        return 0.0f;
    }

    const double clamped = static_cast<double>(clamp_float(percentile, 0.0f, 1.0f));
    const uint64_t target = static_cast<uint64_t>(clamped * static_cast<double>(metrics->sample_count - 1u)) + 1u;
    uint64_t seen = 0u;
    for (uint32_t bucket = 0; bucket < kHistogramBins; ++bucket)
    {
        seen += metrics->histogram[bucket];
        if (seen >= target)
        {
            return bucket_high_ms(bucket);
        }
    }

    return metrics->worst_ms;
}

auto confirmed_low(
    const frame_metrics* metrics,
    uint32_t required_hits,
    uint32_t* out_hits) -> frame_metric_pair
{
    if (out_hits != nullptr)
    {
        *out_hits = 0u;
    }
    if (metrics == nullptr || metrics->sample_count == 0u)
    {
        return {};
    }

    for (int32_t bucket = static_cast<int32_t>(kHistogramBins) - 1; bucket >= 0; --bucket)
    {
        const uint32_t hits = metrics->histogram[static_cast<uint32_t>(bucket)];
        if (hits >= required_hits)
        {
            if (out_hits != nullptr)
            {
                *out_hits = hits;
            }
            return make_pair(bucket_high_ms(static_cast<uint32_t>(bucket)));
        }
    }

    return {};
}

auto warmup_finished(const frame_metrics* metrics, uint64_t now_ticks) -> bool
{
    return metrics != nullptr &&
        metrics->first_sample_ticks != 0u &&
        now_ticks >= metrics->first_sample_ticks + kWarmupNs;
}

auto warmup_elapsed_seconds(const frame_metrics* metrics, uint64_t now_ticks) -> float
{
    if (metrics == nullptr || metrics->first_sample_ticks == 0u || now_ticks <= metrics->first_sample_ticks)
    {
        return 0.0f;
    }

    return min_float(static_cast<float>(now_ticks - metrics->first_sample_ticks) * 1.0e-9f, kWarmupSeconds);
}

} // namespace

void frame_metrics_init(frame_metrics* metrics)
{
    if (metrics == nullptr)
    {
        return;
    }

    *metrics = {};
}

void frame_metrics_record(
    frame_metrics* metrics,
    float frame_ms,
    uint64_t sample_ticks)
{
    if (metrics == nullptr || frame_ms <= 0.0f || sample_ticks == 0u)
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
    metrics->worst_ms = max_float(metrics->worst_ms, frame_ms);
    ++metrics->histogram[bucket_for_ms(frame_ms)];
}

frame_metrics_snapshot frame_metrics_snapshot_value(
    const frame_metrics* metrics,
    uint64_t now_ticks)
{
    if (metrics == nullptr)
    {
        return {};
    }
    if (metrics->first_sample_ticks != 0u &&
        (now_ticks == 0u || now_ticks < metrics->first_sample_ticks))
    {
        now_ticks = metrics->last_sample_ticks;
    }

    frame_metrics_snapshot snapshot{};
    snapshot.current = metrics->current;
    snapshot.warmup_seconds = kWarmupSeconds;
    snapshot.warmup_elapsed_seconds = warmup_elapsed_seconds(metrics, now_ticks);
    snapshot.warmup_complete = warmup_finished(metrics, now_ticks) ? 1u : 0u;
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
