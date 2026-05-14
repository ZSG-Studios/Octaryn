#pragma once

#if defined(OCTARYN_ENABLE_TRACY)
#include <tracy/Tracy.hpp>

#define OCT_PROFILE_ZONE(name) ZoneScopedN(name)
#define OCT_PROFILE_ZONE_END
#define OCT_PROFILE_FRAME() FrameMark
#else
#define OCT_PROFILE_ZONE(name)
#define OCT_PROFILE_ZONE_END
#define OCT_PROFILE_FRAME()
#endif

#include <SDL3/SDL.h>

#include "core/log.h"

inline Uint64 oct_profile_now_ticks(void)
{
    const Uint64 counter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0u)
    {
        return SDL_GetTicksNS();
    }
    return static_cast<Uint64>((static_cast<long double>(counter) * 1000000000.0L) /
                               static_cast<long double>(frequency));
}

inline float oct_profile_elapsed_ms(Uint64 start_ticks)
{
    return (float) (oct_profile_now_ticks() - start_ticks) * 1e-6f;
}

inline void oct_profile_log_duration(const char* category, const char* label, Uint64 start_ticks)
{
    oct_log_infof("%s | %s took %.2f ms", category, label, oct_profile_elapsed_ms(start_ticks));
}
