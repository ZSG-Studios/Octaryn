#include "octaryn_native_profile.h"

#include <chrono>

#include "octaryn_native_log.h"

uint64_t octaryn_native_profile_now_ticks(void)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

float octaryn_native_profile_elapsed_ms(uint64_t start_ticks)
{
    const uint64_t elapsed = octaryn_native_profile_now_ticks() - start_ticks;
    return static_cast<float>(elapsed) * 1.0e-6f;
}

void octaryn_native_profile_log_duration(const char* category, const char* label, uint64_t start_ticks)
{
    octaryn_native_log_infof("%s | %s took %.2f ms", category, label, octaryn_native_profile_elapsed_ms(start_ticks));
}
