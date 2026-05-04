#pragma once

#include <stdint.h>

#ifndef OCTARYN_ABI_CALL
#if defined(_MSC_VER)
#define OCTARYN_ABI_CALL __cdecl
#else
#define OCTARYN_ABI_CALL
#endif
#endif

#define OCTARYN_HOST_COMMAND_SIZE 96u
#define OCTARYN_HOST_COMMAND_CRITICAL_FLAG 1u
#define OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG 2u
#define OCTARYN_HOST_INPUT_SNAPSHOT_SIZE 56u
#define OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE 24u
#define OCTARYN_HOST_FRAME_SNAPSHOT_SIZE 88u
#define OCTARYN_CLIENT_COMMAND_FRAME_SIZE 32u
#define OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE 40u
#define OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE 56u
#define OCTARYN_CHUNK_COLUMN_SNAPSHOT_COLUMN_SIZE 32u
#define OCTARYN_CHUNK_COLUMN_SNAPSHOT_BLOCK_SIZE 24u
#define OCTARYN_REPLICATION_CHANGE_SIZE 40u
#define OCTARYN_NETWORK_MESSAGE_HEADER_SIZE 40u
#define OCTARYN_CLIENT_NATIVE_HOST_API_SIZE 16u
#define OCTARYN_SERVER_NATIVE_HOST_API_SIZE 48u

typedef struct octaryn_host_command {
    uint32_t version;
    uint32_t size;
    uint32_t kind;
    uint32_t flags;
    uint64_t request_id;
    uint64_t target_id;
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    float x;
    float y;
    float z;
    float w;
    float x2;
    float y2;
    float z2;
    float w2;
    uint64_t payload0;
    uint64_t payload1;
} octaryn_host_command;

typedef struct octaryn_host_input_snapshot {
    uint32_t version;
    uint32_t size;
    uint32_t flags;
    uint32_t controller;
    float move_x;
    float move_y;
    float move_z;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_pitch;
    float camera_yaw;
    int32_t relative_mouse;
    int32_t reserved;
} octaryn_host_input_snapshot;

typedef struct octaryn_host_frame_timing_snapshot {
    uint32_t version;
    uint32_t size;
    uint64_t frame_index;
    double delta_seconds;
} octaryn_host_frame_timing_snapshot;

typedef struct octaryn_host_frame_snapshot {
    uint32_t version;
    uint32_t size;
    octaryn_host_input_snapshot input;
    octaryn_host_frame_timing_snapshot timing;
} octaryn_host_frame_snapshot;

typedef struct octaryn_client_command_frame {
    uint32_t version;
    uint32_t size;
    uint32_t command_count;
    uint32_t reserved;
    uint64_t tick_id;
    uint64_t commands_address;
} octaryn_client_command_frame;

typedef struct octaryn_server_snapshot_header {
    uint32_t version;
    uint32_t size;
    uint32_t replication_count;
    uint32_t change_count;
    uint64_t tick_id;
    uint64_t replication_ids_address;
    uint64_t changes_address;
} octaryn_server_snapshot_header;

typedef struct octaryn_chunk_column_request_frame {
    uint32_t version;
    uint32_t size;
    int32_t center_chunk_x;
    int32_t center_chunk_z;
    uint32_t radius;
    uint32_t column_capacity;
    uint32_t block_capacity;
    uint32_t column_count;
    uint32_t block_count;
    uint32_t status;
    uint64_t columns_address;
    uint64_t blocks_address;
} octaryn_chunk_column_request_frame;

typedef struct octaryn_chunk_column_snapshot_column {
    uint32_t version;
    uint32_t size;
    int32_t chunk_x;
    int32_t chunk_z;
    int32_t origin_x;
    int32_t origin_z;
    uint32_t block_offset;
    uint32_t block_count;
} octaryn_chunk_column_snapshot_column;

typedef struct octaryn_chunk_column_snapshot_block {
    uint32_t version;
    uint32_t size;
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t block;
    uint16_t reserved;
} octaryn_chunk_column_snapshot_block;

typedef struct octaryn_replication_change {
    uint32_t version;
    uint32_t size;
    uint32_t change_kind;
    uint32_t flags;
    uint64_t replication_id;
    uint64_t payload0;
    uint64_t payload1;
} octaryn_replication_change;

typedef struct octaryn_network_message_header {
    uint32_t version;
    uint32_t size;
    uint32_t message_kind;
    uint32_t flags;
    uint64_t tick_id;
    uint64_t replication_id;
    uint64_t payload_bytes;
} octaryn_network_message_header;

typedef int (OCTARYN_ABI_CALL* octaryn_enqueue_host_command_fn)(octaryn_host_command* command);
typedef int (OCTARYN_ABI_CALL* octaryn_publish_server_snapshot_fn)(octaryn_server_snapshot_header* snapshot);
typedef int (OCTARYN_ABI_CALL* octaryn_poll_client_commands_fn)(octaryn_client_command_frame* frame);

typedef struct octaryn_client_native_host_api {
    uint32_t version;
    uint32_t size;
    octaryn_enqueue_host_command_fn enqueue_command;
} octaryn_client_native_host_api;

typedef struct octaryn_server_native_host_api {
    uint32_t version;
    uint32_t size;
    octaryn_enqueue_host_command_fn enqueue_host_command;
    octaryn_publish_server_snapshot_fn publish_server_snapshot;
    octaryn_poll_client_commands_fn poll_client_commands;
    uint64_t reserved;
    uint64_t reserved1;
} octaryn_server_native_host_api;
