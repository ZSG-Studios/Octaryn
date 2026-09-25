#pragma once

#include <stdint.h>

#if defined(OCTARYN_NATIVE_PROFILING_USE_TRACY) && defined(__cplusplus)
#include <tracy/Tracy.hpp>
#define OCTARYN_NATIVE_PROFILE_ZONE(name) ZoneScopedN(name)
#define OCTARYN_NATIVE_PROFILE_FRAME() FrameMark
#else
#define OCTARYN_NATIVE_PROFILE_ZONE(name)
#define OCTARYN_NATIVE_PROFILE_FRAME()
#endif

#ifdef __cplusplus
extern "C" {
#endif

uint64_t octaryn_native_profile_now_ticks(void);
float octaryn_native_profile_elapsed_ms(uint64_t start_ticks);
void octaryn_native_profile_log_duration(const char* category, const char* label, uint64_t start_ticks);

#ifdef __cplusplus
}
#endif
