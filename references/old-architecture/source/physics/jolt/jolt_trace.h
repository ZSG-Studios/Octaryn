#pragma once

#include <Jolt/Jolt.h>

namespace octaryn::physics {

void jolt_trace_callback(const char* format, ...);

#if defined(JPH_ENABLE_ASSERTS)
bool jolt_assert_callback(const char* expression, const char* message, const char* file, unsigned int line);
#endif

} // namespace octaryn::physics
