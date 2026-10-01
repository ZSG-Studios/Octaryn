#pragma once

#include "octaryn_host_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_initialize(octaryn_server_native_host_api* native_api);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_tick(octaryn_host_frame_snapshot* frame_snapshot);
OCTARYN_ABI_EXPORT void OCTARYN_ABI_CALL octaryn_server_shutdown(void);

#ifdef __cplusplus
}
#endif
