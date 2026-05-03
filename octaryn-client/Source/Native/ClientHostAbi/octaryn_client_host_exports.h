#pragma once

#include "octaryn_host_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_client_initialize(octaryn_client_native_host_api* native_api);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_client_tick(octaryn_host_frame_snapshot* frame_snapshot);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_client_apply_server_snapshot(octaryn_server_snapshot_header* snapshot_header);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_client_drain_presentation_updates(
    octaryn_replication_change* changes,
    uint32_t capacity,
    uint32_t* written);
OCTARYN_ABI_EXPORT void OCTARYN_ABI_CALL octaryn_client_shutdown(void);

#ifdef __cplusplus
}
#endif
