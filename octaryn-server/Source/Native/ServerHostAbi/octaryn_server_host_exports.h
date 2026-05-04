#pragma once

#include "octaryn_host_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_initialize(octaryn_server_native_host_api* native_api);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_tick(octaryn_host_frame_snapshot* frame_snapshot);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_submit_client_commands(octaryn_client_command_frame* command_frame);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_drain_server_snapshots(octaryn_server_snapshot_header* snapshot_header);
OCTARYN_ABI_EXPORT int OCTARYN_ABI_CALL octaryn_server_request_chunk_columns(octaryn_chunk_column_request_frame* request_frame);
OCTARYN_ABI_EXPORT void OCTARYN_ABI_CALL octaryn_server_shutdown(void);

#ifdef __cplusplus
}
#endif
