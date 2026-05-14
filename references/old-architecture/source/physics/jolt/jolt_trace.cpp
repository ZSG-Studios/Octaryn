#include "physics/jolt/jolt_trace.h"

#include <cstdarg>
#include <cstdio>

#include "core/log.h"

namespace octaryn::physics {

void jolt_trace_callback(const char* format, ...)
{
    char message[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    oct_log_infof("Jolt: %s", message);
}

#if defined(JPH_ENABLE_ASSERTS)
bool jolt_assert_callback(const char* expression, const char* message, const char* file, unsigned int line)
{
    oct_log_errorf("Jolt assert: %s:%u: (%s) %s", file, line, expression, message ? message : "");
    return true;
}
#endif

} // namespace octaryn::physics
