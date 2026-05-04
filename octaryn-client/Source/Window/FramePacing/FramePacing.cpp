#include "FramePacing.h"

#include <algorithm>
#include <limits>

namespace {

constexpr int DefaultFpsCap = 0;
constexpr int DefaultFpsCapSpinUs = 3000;
constexpr int DefaultFramesInFlight = 3;
constexpr int DefaultSwapchainUnavailableSleepUs = 2000;
constexpr int MaxFramesInFlight = 3;

const char* environment_value(const char* name)
{
    const char* value = SDL_getenv(name);
    return value != nullptr && value[0] != '\0' ? value : nullptr;
}

bool environment_equals(const char* value, const char* expected)
{
    return value != nullptr && SDL_strcasecmp(value, expected) == 0;
}

present_mode_policy parse_present_mode_policy(const char* value)
{
    if (value == nullptr)
    {
        return PRESENT_MODE_POLICY_IMMEDIATE;
    }
    if (environment_equals(value, "immediate"))
    {
        return PRESENT_MODE_POLICY_IMMEDIATE;
    }
    if (environment_equals(value, "mailbox"))
    {
        return PRESENT_MODE_POLICY_MAILBOX;
    }
    if (environment_equals(value, "vsync"))
    {
        return PRESENT_MODE_POLICY_VSYNC;
    }

    return PRESENT_MODE_POLICY_AUTO;
}

swapchain_acquire_mode parse_acquire_mode(const char* value)
{
    if (value == nullptr)
    {
        return SWAPCHAIN_ACQUIRE_NONBLOCKING;
    }
    if (environment_equals(value, "early") ||
        environment_equals(value, "wait") ||
        environment_equals(value, "blocking"))
    {
        return SWAPCHAIN_ACQUIRE_EARLY;
    }
    if (environment_equals(value, "late") ||
        environment_equals(value, "late_swapchain"))
    {
        return SWAPCHAIN_ACQUIRE_LATE;
    }
    if (environment_equals(value, "nonblocking") ||
        environment_equals(value, "try") ||
        environment_equals(value, "try_swapchain"))
    {
        return SWAPCHAIN_ACQUIRE_NONBLOCKING;
    }

    return SWAPCHAIN_ACQUIRE_NONBLOCKING;
}

int parse_nonnegative_int(const char* value, int fallback)
{
    if (value == nullptr)
    {
        return fallback;
    }

    char* end = nullptr;
    const long parsed = SDL_strtol(value, &end, 10);
    if (end == value || parsed < 0 || parsed > static_cast<long>(std::numeric_limits<int>::max()))
    {
        return fallback;
    }

    return static_cast<int>(parsed);
}

SDL_GPUPresentMode present_mode_from_policy(present_mode_policy policy)
{
    switch (policy)
    {
    case PRESENT_MODE_POLICY_IMMEDIATE: return SDL_GPU_PRESENTMODE_IMMEDIATE;
    case PRESENT_MODE_POLICY_MAILBOX: return SDL_GPU_PRESENTMODE_MAILBOX;
    case PRESENT_MODE_POLICY_VSYNC: return SDL_GPU_PRESENTMODE_VSYNC;
    case PRESENT_MODE_POLICY_AUTO:
    default: return SDL_GPU_PRESENTMODE_VSYNC;
    }
}

bool window_supports_present_mode(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUPresentMode present_mode)
{
    return SDL_WindowSupportsGPUPresentMode(device, window, present_mode);
}

} // namespace

void frame_pacing_init(frame_pacing* state)
{
    if (state == nullptr)
    {
        return;
    }

    *state = {};
    state->requested_present_mode = parse_present_mode_policy(environment_value("OCTARYN_PRESENT_MODE"));
    state->actual_present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    state->acquire_mode = parse_acquire_mode(environment_value("OCTARYN_ACQUIRE_MODE"));
    state->fps_cap = parse_nonnegative_int(environment_value("OCTARYN_FPS_CAP"), DefaultFpsCap);
    state->fps_cap_spin_us = parse_nonnegative_int(
        environment_value("OCTARYN_FPS_CAP_SPIN_US"),
        DefaultFpsCapSpinUs);
    state->allowed_frames_in_flight = std::clamp(
        parse_nonnegative_int(environment_value("OCTARYN_FRAMES_IN_FLIGHT"), DefaultFramesInFlight),
        1,
        MaxFramesInFlight);
    state->swapchain_unavailable_sleep_us = parse_nonnegative_int(
        environment_value("OCTARYN_SWAPCHAIN_SKIP_SLEEP_US"),
        DefaultSwapchainUnavailableSleepUs);
}

void frame_pacing_set_actual_present_mode(
    frame_pacing* state,
    SDL_GPUPresentMode present_mode)
{
    if (state != nullptr)
    {
        state->actual_present_mode = present_mode;
    }
}

SDL_GPUPresentMode frame_pacing_choose_present_mode(
    const frame_pacing* state,
    SDL_GPUDevice* device,
    SDL_Window* window)
{
    const present_mode_policy policy =
        state != nullptr ? state->requested_present_mode : PRESENT_MODE_POLICY_AUTO;
    if (policy != PRESENT_MODE_POLICY_AUTO)
    {
        const SDL_GPUPresentMode forced_mode = present_mode_from_policy(policy);
        if (window_supports_present_mode(device, window, forced_mode))
        {
            return forced_mode;
        }
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

int frame_pacing_should_defer_swapchain_acquire(
    const frame_pacing* state)
{
    return state != nullptr && state->acquire_mode != SWAPCHAIN_ACQUIRE_EARLY ? 1 : 0;
}

int frame_pacing_should_probe_swapchain_before_scene(
    const frame_pacing* state)
{
    return state != nullptr && state->acquire_mode == SWAPCHAIN_ACQUIRE_NONBLOCKING ? 1 : 0;
}

float frame_pacing_sleep_until_next_frame(
    frame_pacing* state,
    Uint64 frame_start_ticks)
{
    if (state == nullptr || state->fps_cap <= 0 || frame_start_ticks == 0u)
    {
        if (state != nullptr)
        {
            state->next_frame_target_ticks = 0;
        }
        return 0.0f;
    }

    const Uint64 frame_interval_ns = SDL_NS_PER_SECOND / static_cast<Uint64>(state->fps_cap);
    state->next_frame_target_ticks = frame_start_ticks + frame_interval_ns;

    const Uint64 wait_start = SDL_GetTicksNS();
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

    while ((now = SDL_GetTicksNS()) < state->next_frame_target_ticks)
    {
        SDL_CPUPauseInstruction();
    }

    return now > wait_start ? static_cast<float>(now - wait_start) * 1e-6f : 0.0f;
}

float frame_pacing_sleep_after_swapchain_unavailable(
    const frame_pacing* state)
{
    if (state == nullptr ||
        state->acquire_mode != SWAPCHAIN_ACQUIRE_NONBLOCKING ||
        state->swapchain_unavailable_sleep_us <= 0)
    {
        return 0.0f;
    }

    const Uint64 sleep_ns = static_cast<Uint64>(state->swapchain_unavailable_sleep_us) * 1000ull;
    SDL_DelayNS(sleep_ns);
    return static_cast<float>(state->swapchain_unavailable_sleep_us) * 0.001f;
}

const char* frame_pacing_present_policy_name(
    present_mode_policy policy)
{
    switch (policy)
    {
    case PRESENT_MODE_POLICY_AUTO: return "auto";
    case PRESENT_MODE_POLICY_IMMEDIATE: return "immediate";
    case PRESENT_MODE_POLICY_MAILBOX: return "mailbox";
    case PRESENT_MODE_POLICY_VSYNC: return "vsync";
    default: return "unknown";
    }
}

const char* frame_pacing_acquire_mode_name(
    swapchain_acquire_mode mode)
{
    switch (mode)
    {
    case SWAPCHAIN_ACQUIRE_EARLY: return "early";
    case SWAPCHAIN_ACQUIRE_LATE: return "late_swapchain";
    case SWAPCHAIN_ACQUIRE_NONBLOCKING: return "nonblocking";
    default: return "unknown";
    }
}
