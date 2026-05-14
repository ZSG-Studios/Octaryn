#include "app/runtime/frame_pacing.h"

#include "app/runtime/diagnostics.h"
#include "core/log.h"
#include "core/profile.h"

namespace {

constexpr int kDefaultFpsCap = 0;
constexpr int kDefaultFpsCapSpinUs = 3000;
constexpr int kDefaultFramesInFlight = 3;
constexpr int kDefaultSwapchainUnavailableSleepUs = 2000;

auto env_value(const char* name) -> const char*
{
    const char* value = SDL_getenv(name);
    return value && value[0] != '\0' ? value : nullptr;
}

auto env_equals(const char* value, const char* expected) -> bool
{
    return value && SDL_strcasecmp(value, expected) == 0;
}

auto parse_present_mode_policy(const char* value) -> app_present_mode_policy_t
{
    if (!value)
    {
        return APP_PRESENT_MODE_POLICY_IMMEDIATE;
    }
    if (env_equals(value, "immediate"))
    {
        return APP_PRESENT_MODE_POLICY_IMMEDIATE;
    }
    if (env_equals(value, "mailbox"))
    {
        return APP_PRESENT_MODE_POLICY_MAILBOX;
    }
    if (env_equals(value, "vsync"))
    {
        return APP_PRESENT_MODE_POLICY_VSYNC;
    }
    return APP_PRESENT_MODE_POLICY_AUTO;
}

auto parse_acquire_mode(const char* value) -> app_swapchain_acquire_mode_t
{
    if (!value)
    {
        return APP_SWAPCHAIN_ACQUIRE_NONBLOCKING;
    }
    if (env_equals(value, "early") || env_equals(value, "wait") || env_equals(value, "blocking"))
    {
        return APP_SWAPCHAIN_ACQUIRE_EARLY;
    }
    if (env_equals(value, "nonblocking") || env_equals(value, "try") || env_equals(value, "try_swapchain"))
    {
        return APP_SWAPCHAIN_ACQUIRE_NONBLOCKING;
    }
    if (env_equals(value, "late") || env_equals(value, "late_swapchain"))
    {
        return APP_SWAPCHAIN_ACQUIRE_LATE;
    }
    return APP_SWAPCHAIN_ACQUIRE_NONBLOCKING;
}

auto parse_nonnegative_int(const char* value, int fallback) -> int
{
    if (!value)
    {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = SDL_strtol(value, &end, 10);
    if (end == value || parsed < 0)
    {
        return fallback;
    }
    return static_cast<int>(parsed);
}

auto present_mode_from_policy(app_present_mode_policy_t policy) -> SDL_GPUPresentMode
{
    switch (policy)
    {
    case APP_PRESENT_MODE_POLICY_IMMEDIATE: return SDL_GPU_PRESENTMODE_IMMEDIATE;
    case APP_PRESENT_MODE_POLICY_MAILBOX: return SDL_GPU_PRESENTMODE_MAILBOX;
    case APP_PRESENT_MODE_POLICY_VSYNC: return SDL_GPU_PRESENTMODE_VSYNC;
    case APP_PRESENT_MODE_POLICY_AUTO:
    default: return SDL_GPU_PRESENTMODE_VSYNC;
    }
}

auto window_supports_present_mode(SDL_GPUDevice* device, SDL_Window* window, SDL_GPUPresentMode present_mode) -> bool
{
    return SDL_WindowSupportsGPUPresentMode(device, window, present_mode);
}

} // namespace

void app_frame_pacing_init(app_frame_pacing_t* state)
{
    if (!state)
    {
        return;
    }

    *state = {};
    state->requested_present_mode = parse_present_mode_policy(env_value("OCTARYN_PRESENT_MODE"));
    state->actual_present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    state->acquire_mode = parse_acquire_mode(env_value("OCTARYN_ACQUIRE_MODE"));
    state->fps_cap = parse_nonnegative_int(env_value("OCTARYN_FPS_CAP"), kDefaultFpsCap);
    state->fps_cap_spin_us = parse_nonnegative_int(env_value("OCTARYN_FPS_CAP_SPIN_US"), kDefaultFpsCapSpinUs);
    state->allowed_frames_in_flight =
        SDL_clamp(parse_nonnegative_int(env_value("OCTARYN_FRAMES_IN_FLIGHT"), kDefaultFramesInFlight), 1, 3);
    state->swapchain_unavailable_sleep_us =
        parse_nonnegative_int(env_value("OCTARYN_SWAPCHAIN_SKIP_SLEEP_US"), kDefaultSwapchainUnavailableSleepUs);
    state->next_frame_target_ticks = 0;

    oct_log_infof("Frame pacing | requested_present=%s acquire=%s fps_cap=%d fps_cap_spin_us=%d frames_in_flight=%d swapchain_skip_sleep_us=%d",
                  app_frame_pacing_present_policy_name(state->requested_present_mode),
                  app_frame_pacing_acquire_mode_name(state->acquire_mode),
                  state->fps_cap,
                  state->fps_cap_spin_us,
                  state->allowed_frames_in_flight,
                  state->swapchain_unavailable_sleep_us);
}

void app_frame_pacing_set_actual_present_mode(app_frame_pacing_t* state, SDL_GPUPresentMode present_mode)
{
    if (!state)
    {
        return;
    }
    state->actual_present_mode = present_mode;
}

SDL_GPUPresentMode app_frame_pacing_choose_present_mode(const app_frame_pacing_t* state,
                                                        SDL_GPUDevice* device,
                                                        SDL_Window* window)
{
    const app_present_mode_policy_t policy = state ? state->requested_present_mode : APP_PRESENT_MODE_POLICY_AUTO;
    if (policy != APP_PRESENT_MODE_POLICY_AUTO)
    {
        const SDL_GPUPresentMode forced_mode = present_mode_from_policy(policy);
        if (window_supports_present_mode(device, window, forced_mode))
        {
            return forced_mode;
        }
        oct_log_warnf("Requested GPU present mode %s is unsupported; falling back to auto",
                      app_frame_pacing_present_policy_name(policy));
    }

    if (window_supports_present_mode(device, window, SDL_GPU_PRESENTMODE_IMMEDIATE))
    {
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    if (window_supports_present_mode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        return SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (window_supports_present_mode(device, window, SDL_GPU_PRESENTMODE_VSYNC))
    {
        return SDL_GPU_PRESENTMODE_VSYNC;
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

bool app_frame_pacing_should_defer_swapchain_acquire(const app_frame_pacing_t* state)
{
    if (!state)
    {
        return false;
    }
    return state->acquire_mode != APP_SWAPCHAIN_ACQUIRE_EARLY;
}

bool app_frame_pacing_should_probe_swapchain_before_scene(const app_frame_pacing_t* state)
{
    return state && state->acquire_mode == APP_SWAPCHAIN_ACQUIRE_NONBLOCKING;
}

float app_frame_pacing_sleep_until_next_frame(app_frame_pacing_t* state, Uint64 frame_start_ticks)
{
    if (!state || state->fps_cap <= 0 || frame_start_ticks == 0u)
    {
        if (state)
        {
            state->next_frame_target_ticks = 0;
        }
        return 0.0f;
    }

    const Uint64 frame_interval_ns = SDL_NS_PER_SECOND / static_cast<Uint64>(state->fps_cap);
    state->next_frame_target_ticks = frame_start_ticks + frame_interval_ns;

    const Uint64 wait_start = oct_profile_now_ticks();
    Uint64 now = wait_start;
    if (now >= state->next_frame_target_ticks)
    {
        return 0.0f;
    }

    const Uint64 spin_window_ns = static_cast<Uint64>(state->fps_cap_spin_us) * 1000ull;
    if (state->next_frame_target_ticks > now + spin_window_ns)
    {
        SDL_DelayPrecise(state->next_frame_target_ticks - now - spin_window_ns);
    }

    while ((now = oct_profile_now_ticks()) < state->next_frame_target_ticks)
    {
        SDL_CPUPauseInstruction();
    }

    return now > wait_start ? static_cast<float>(now - wait_start) * 1e-6f : 0.0f;
}

float app_frame_pacing_sleep_after_swapchain_unavailable(const app_frame_pacing_t* state)
{
    if (!state ||
        state->acquire_mode != APP_SWAPCHAIN_ACQUIRE_NONBLOCKING ||
        state->swapchain_unavailable_sleep_us <= 0)
    {
        return 0.0f;
    }

    const Uint64 sleep_ns = static_cast<Uint64>(state->swapchain_unavailable_sleep_us) * 1000ull;
    SDL_DelayNS(sleep_ns);
    return static_cast<float>(state->swapchain_unavailable_sleep_us) * 0.001f;
}

const char* app_frame_pacing_present_policy_name(app_present_mode_policy_t policy)
{
    switch (policy)
    {
    case APP_PRESENT_MODE_POLICY_AUTO: return "auto";
    case APP_PRESENT_MODE_POLICY_IMMEDIATE: return "immediate";
    case APP_PRESENT_MODE_POLICY_MAILBOX: return "mailbox";
    case APP_PRESENT_MODE_POLICY_VSYNC: return "vsync";
    default: return "unknown";
    }
}

const char* app_frame_pacing_acquire_mode_name(app_swapchain_acquire_mode_t mode)
{
    switch (mode)
    {
    case APP_SWAPCHAIN_ACQUIRE_EARLY: return "early";
    case APP_SWAPCHAIN_ACQUIRE_LATE: return "late_swapchain";
    case APP_SWAPCHAIN_ACQUIRE_NONBLOCKING: return "nonblocking";
    default: return "unknown";
    }
}
