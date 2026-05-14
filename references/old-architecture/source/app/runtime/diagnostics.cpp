#include "app/runtime/diagnostics.h"

#include "app/runtime/internal.h"
#include "core/log.h"
#include "core/profile.h"

namespace {

constexpr Uint64 kProfileLogIntervalNs = 5ull * SDL_NS_PER_SECOND;
Uint64 g_last_profile_summary_ticks = 0;

const char* sdl_log_category_name(int category)
{
    switch (category)
    {
    case SDL_LOG_CATEGORY_APPLICATION: return "application";
    case SDL_LOG_CATEGORY_ERROR: return "error";
    case SDL_LOG_CATEGORY_ASSERT: return "assert";
    case SDL_LOG_CATEGORY_SYSTEM: return "system";
    case SDL_LOG_CATEGORY_AUDIO: return "audio";
    case SDL_LOG_CATEGORY_VIDEO: return "video";
    case SDL_LOG_CATEGORY_RENDER: return "render";
    case SDL_LOG_CATEGORY_INPUT: return "input";
    case SDL_LOG_CATEGORY_TEST: return "test";
    case SDL_LOG_CATEGORY_GPU: return "gpu";
    default: return "other";
    }
}

void sdl_log_bridge(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    (void) userdata;
    if (gpu_validation_enabled &&
        category == SDL_LOG_CATEGORY_GPU &&
        message &&
        SDL_strstr(message, "Validation layers not found, continuing without validation"))
    {
        gpu_validation_unavailable = true;
    }

    const char* category_name = sdl_log_category_name(category);
    switch (priority)
    {
    case SDL_LOG_PRIORITY_VERBOSE:
    case SDL_LOG_PRIORITY_DEBUG:
        oct_log_debugf("[SDL][%s] %s", category_name, message);
        break;
    case SDL_LOG_PRIORITY_WARN:
        oct_log_warnf("[SDL][%s] %s", category_name, message);
        break;
    case SDL_LOG_PRIORITY_ERROR:
    case SDL_LOG_PRIORITY_CRITICAL:
        oct_log_errorf("[SDL][%s] %s", category_name, message);
        break;
    case SDL_LOG_PRIORITY_INFO:
    default:
        oct_log_infof("[SDL][%s] %s", category_name, message);
        break;
    }
}

SDL_AssertState SDLCALL sdl_assertion_bridge(const SDL_AssertData* data, void* userdata)
{
    (void) userdata;

    const char* condition = (data && data->condition) ? data->condition : "<unknown>";
    const char* function = (data && data->function) ? data->function : "<unknown>";
    const char* filename = (data && data->filename) ? data->filename : "<unknown>";
    const int line = data ? data->linenum : 0;
    const unsigned int trigger_count = data ? data->trigger_count : 0;

    oct_log_errorf("[SDL][assert] condition=%s function=%s file=%s:%d trigger_count=%u",
                   condition,
                   function,
                   filename,
                   line,
                   trigger_count);

    return SDL_ASSERTION_ABORT;
}

bool env_flag_enabled(const char* name)
{
    const char* value = SDL_getenv(name);
    return value && value[0] != '\0' && SDL_strcmp(value, "0") != 0 && SDL_strcasecmp(value, "false") != 0 &&
        SDL_strcasecmp(value, "off") != 0;
}

float env_float_clamped(const char* name, float fallback, float minimum, float maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }
    const float parsed = static_cast<float>(SDL_atof(value));
    return SDL_clamp(parsed, minimum, maximum);
}

void log_profile_summary(void)
{
    const app_frame_metrics_snapshot_t metrics = main_frame_profile_metrics_snapshot(&frame_profile);
    oct_log_infof("Frame summary 5s: warmup=%u warmup_elapsed=%.2f warmup_total=%.2f samples=%llu current_ms=%.2f current_fps=%.1f avg_ms=%.2f avg_fps=%.1f low_1pct_ms=%.2f low_1pct_fps=%.1f low_0_1pct_ms=%.2f low_0_1pct_fps=%.1f low_x5_ms=%.2f low_x5_fps=%.1f low_x5_hits=%u low_x10_ms=%.2f low_x10_fps=%.1f low_x10_hits=%u worst_ms=%.2f worst_fps=%.1f",
                  metrics.warmup_complete ? 1u : 0u,
                  metrics.warmup_elapsed_seconds,
                  metrics.warmup_seconds,
                  static_cast<unsigned long long>(metrics.sample_count),
                  metrics.current.ms,
                  metrics.current.fps,
                  metrics.average.ms,
                  metrics.average.fps,
                  metrics.low_1pct.ms,
                  metrics.low_1pct.fps,
                  metrics.low_0_1pct.ms,
                  metrics.low_0_1pct.fps,
                  metrics.confirmed_low_5.ms,
                  metrics.confirmed_low_5.fps,
                  metrics.confirmed_low_5_hits,
                  metrics.confirmed_low_10.ms,
                  metrics.confirmed_low_10.fps,
                  metrics.confirmed_low_10_hits,
                  metrics.worst.ms,
                  metrics.worst.fps);
}

}  // namespace

void app_configure_sdl_logging(void)
{
    SDL_SetLogOutputFunction(sdl_log_bridge, NULL);
    SDL_SetAssertionHandler(sdl_assertion_bridge, NULL);
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#else
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif
}

Uint64 app_profile_now(void)
{
    return oct_profile_now_ticks();
}

float app_profile_elapsed_ms(Uint64 start_ticks)
{
    return oct_profile_elapsed_ms(start_ticks);
}

void app_maybe_log_profile_spike(const main_frame_profile_sample_t* sample)
{
    if (!sample || !env_flag_enabled("OCTARYN_LOG_RENDER_PROFILE"))
    {
        return;
    }

    const Uint64 now = app_profile_now();
    if (g_last_profile_summary_ticks == 0u)
    {
        g_last_profile_summary_ticks = now;
    }

    const float spike_threshold_ms = env_float_clamped("OCTARYN_FRAME_SPIKE_LOG_MS", 30.0f, 1.0f, 1000.0f);
    if (sample->total_ms >= spike_threshold_ms && now - last_profile_spike_log_ticks >= SDL_NS_PER_SECOND)
    {
        last_profile_spike_log_ticks = now;
        oct_log_infof("Frame spike: total=%.2fms accounted=%.2f untracked=%.2f misc=%.2f world=%.2f render=%.2f render_setup=%.2f gbuffer=%.2f post=%.2f forward=%.2f ui=%.2f submit=%.2f acquire=%.2f command=%.2f swap_wait=%.2f fps_sleep=%.2f app_gap=%.2f post_submit_tail=%.2f submit_gap=%.2f",
                      sample->total_ms,
                      sample->accounted_ms,
                      sample->untracked_ms,
                      sample->misc_ms,
                      sample->world_ms,
                      sample->render_ms,
                      sample->render_setup_ms,
                      sample->gbuffer_ms,
                      sample->post_ms,
                      sample->forward_ms,
                      sample->ui_ms,
                      sample->render_submit_ms,
                      sample->frame_acquire_ms,
                      sample->command_acquire_ms,
                      sample->swapchain_wait_ms,
                      sample->fps_cap_sleep_ms,
                      sample->app_callback_gap_ms,
                      sample->post_submit_tail_ms,
                      sample->submit_gap_ms);
    }
    if (now - g_last_profile_summary_ticks >= kProfileLogIntervalNs)
    {
        g_last_profile_summary_ticks = now;
        log_profile_summary();
    }
}
