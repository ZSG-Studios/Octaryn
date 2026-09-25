#include "octaryn_native_memory.h"

#include <mutex>

#include "octaryn_native_log.h"

#if defined(OCTARYN_NATIVE_MEMORY_USE_MIMALLOC)
#include <mimalloc.h>
#endif

void octaryn_native_memory_init(void)
{
    static std::once_flag once;
    std::call_once(once, []() {
#if defined(OCTARYN_NATIVE_MEMORY_USE_MIMALLOC)
        octaryn_native_log_infof("Native memory initialized with mimalloc %d", mi_version());
#else
        octaryn_native_log_infof("Native memory initialized with platform allocator");
#endif
    });
}

bool octaryn_native_memory_uses_mimalloc(void)
{
#if defined(OCTARYN_NATIVE_MEMORY_USE_MIMALLOC)
    return true;
#else
    return false;
#endif
}
