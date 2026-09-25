#include "octaryn_host_abi.h"

#include <stddef.h>

#define OCTARYN_ASSERT_OFFSET(type, member, expected) \
    _Static_assert(offsetof(type, member) == (expected), #type "." #member " ABI offset mismatch")

_Static_assert(sizeof(octaryn_host_command) == OCTARYN_HOST_COMMAND_SIZE, "octaryn_host_command ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_host_command, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, kind, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, flags, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, request_id, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, target_id, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, a, 32u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, b, 36u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, c, 40u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, d, 44u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, x, 48u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, y, 52u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, z, 56u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, w, 60u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, x2, 64u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, y2, 68u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, z2, 72u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, w2, 76u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, payload0, 80u);
OCTARYN_ASSERT_OFFSET(octaryn_host_command, payload1, 88u);
_Static_assert(sizeof(octaryn_host_input_snapshot) == OCTARYN_HOST_INPUT_SNAPSHOT_SIZE, "octaryn_host_input_snapshot ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, flags, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, controller, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, move_x, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, move_y, 20u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, move_z, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, camera_x, 28u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, camera_y, 32u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, camera_z, 36u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, camera_pitch, 40u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, camera_yaw, 44u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, relative_mouse, 48u);
OCTARYN_ASSERT_OFFSET(octaryn_host_input_snapshot, reserved, 52u);
_Static_assert(sizeof(octaryn_host_frame_timing_snapshot) == OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE, "octaryn_host_frame_timing_snapshot ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_timing_snapshot, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_timing_snapshot, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_timing_snapshot, frame_index, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_timing_snapshot, delta_seconds, 16u);
_Static_assert(sizeof(octaryn_host_frame_snapshot) == OCTARYN_HOST_FRAME_SNAPSHOT_SIZE, "octaryn_host_frame_snapshot ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_snapshot, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_snapshot, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_snapshot, input, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_host_frame_snapshot, timing, 64u);
_Static_assert(sizeof(octaryn_client_command_frame) == OCTARYN_CLIENT_COMMAND_FRAME_SIZE, "octaryn_client_command_frame ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, command_count, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, reserved, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, tick_id, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_client_command_frame, commands_address, 24u);
_Static_assert(sizeof(octaryn_server_snapshot_header) == OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE, "octaryn_server_snapshot_header ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, replication_count, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, change_count, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, tick_id, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, replication_ids_address, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_server_snapshot_header, changes_address, 32u);
_Static_assert(sizeof(octaryn_replication_change) == OCTARYN_REPLICATION_CHANGE_SIZE, "octaryn_replication_change ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, change_kind, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, flags, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, replication_id, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, payload0, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_replication_change, payload1, 32u);
_Static_assert(sizeof(octaryn_network_message_header) == OCTARYN_NETWORK_MESSAGE_HEADER_SIZE, "octaryn_network_message_header ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, message_kind, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, flags, 12u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, tick_id, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, replication_id, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_network_message_header, payload_bytes, 32u);
_Static_assert(sizeof(octaryn_client_native_host_api) == OCTARYN_CLIENT_NATIVE_HOST_API_SIZE, "octaryn_client_native_host_api ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_client_native_host_api, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_client_native_host_api, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_client_native_host_api, enqueue_command, 8u);
_Static_assert(sizeof(octaryn_server_native_host_api) == OCTARYN_SERVER_NATIVE_HOST_API_SIZE, "octaryn_server_native_host_api ABI size mismatch");
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, version, 0u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, size, 4u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, enqueue_host_command, 8u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, publish_server_snapshot, 16u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, poll_client_commands, 24u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, reserved, 32u);
OCTARYN_ASSERT_OFFSET(octaryn_server_native_host_api, reserved1, 40u);

uint32_t octaryn_host_abi_version(void)
{
    return OCTARYN_HOST_ABI_VERSION;
}

uint32_t octaryn_host_min_worker_threads(void)
{
    return OCTARYN_HOST_MIN_WORKER_THREADS;
}
