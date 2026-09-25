#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void octaryn_native_memory_init(void);
bool octaryn_native_memory_uses_mimalloc(void);

#ifdef __cplusplus
}
#endif
