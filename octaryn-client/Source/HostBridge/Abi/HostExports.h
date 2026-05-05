#pragma once

#include "octaryn_host_abi.h"

#define OCTARYN_CLIENT_CHUNK_MESH_UPLOAD_RECORD_SIZE 96u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct octaryn_client_chunk_mesh_upload_record {
    uint32_t version;
    uint32_t size;
    int32_t chunk_x;
    int32_t chunk_y;
    int32_t chunk_z;
    uint32_t flags;
    uint32_t opaque_face_count;
    uint32_t transparent_face_count;
    uint32_t sprite_vertex_count;
    uint32_t sprite_index_count;
    uint32_t fluid_block_count;
    uint32_t reserved;
    uint64_t opaque_face_offset;
    uint64_t transparent_face_offset;
    uint64_t sprite_vertex_offset;
    uint64_t opaque_byte_count;
    uint64_t transparent_byte_count;
    uint64_t sprite_byte_count;
} octaryn_client_chunk_mesh_upload_record;

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
