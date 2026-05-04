#include "HostExports.h"
#include "octaryn_native_crash_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* s_log;

static int OCTARYN_ABI_CALL octaryn_probe_enqueue_host_command(octaryn_host_command* command)
{
    if (s_log != NULL && command != NULL) {
        fprintf(s_log, "enqueue_host_command kind=%u request=%llu\n",
            command->kind,
            (unsigned long long)command->request_id);
    }

    return 1;
}

static int OCTARYN_ABI_CALL octaryn_probe_publish_server_snapshot(octaryn_server_snapshot_header* snapshot)
{
    if (s_log != NULL && snapshot != NULL) {
        fprintf(s_log, "publish_server_snapshot tick=%llu changes=%u\n",
            (unsigned long long)snapshot->tick_id,
            snapshot->change_count);
    }

    return 1;
}

static int OCTARYN_ABI_CALL octaryn_probe_poll_client_commands(octaryn_client_command_frame* frame)
{
    if (s_log != NULL && frame != NULL) {
        fprintf(s_log, "poll_client_commands tick=%llu count=%u\n",
            (unsigned long long)frame->tick_id,
            frame->command_count);
    }

    return 1;
}

static octaryn_host_frame_snapshot octaryn_probe_frame(void)
{
    octaryn_host_frame_snapshot frame = {0};
    frame.version = 1u;
    frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
    frame.input.version = 1u;
    frame.input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
    frame.input.flags = (1u << 0u) | (1u << 1u) | (1u << 2u);
    frame.input.controller = 1u;
    frame.input.move_x = 1.0f;
    frame.input.move_y = 1.0f;
    frame.input.move_z = 1.0f;
    frame.input.camera_pitch = -0.45471975f;
    frame.input.camera_yaw = 0.20943952f;
    frame.input.relative_mouse = 1;
    frame.timing.version = 1u;
    frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
    frame.timing.frame_index = 1u;
    frame.timing.delta_seconds = 1.0 / 60.0;
    return frame;
}

int main(void)
{
    s_log = fopen(OCTARYN_SERVER_LAUNCH_PROBE_LOG_PATH, "w");
    if (s_log == NULL) {
        return 2;
    }

    octaryn_native_crash_diagnostics_init("server-launch-probe");
    fprintf(s_log, "crash_marker=%s\n", octaryn_native_crash_diagnostics_marker_path());

    octaryn_server_native_host_api api = {0};
    api.version = 1u;
    api.size = OCTARYN_SERVER_NATIVE_HOST_API_SIZE;
    api.enqueue_host_command = octaryn_probe_enqueue_host_command;
    api.publish_server_snapshot = octaryn_probe_publish_server_snapshot;
    api.poll_client_commands = octaryn_probe_poll_client_commands;

    octaryn_host_frame_snapshot frame = octaryn_probe_frame();
    int result = octaryn_server_tick(&frame);
    fprintf(s_log, "tick_before_initialize=%d\n", result);
    if (result != -1) {
        fclose(s_log);
        return 3;
    }

    result = octaryn_server_initialize(&api);
    fprintf(s_log, "initialize=%d\n", result);
    if (result != 0) {
        fclose(s_log);
        return 4;
    }

    result = octaryn_server_tick(&frame);
    fprintf(s_log, "tick=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 5;
    }

    result = octaryn_server_initialize(&api);
    fprintf(s_log, "reinitialize=%d\n", result);
    if (result != 0) {
        fclose(s_log);
        return 6;
    }

    result = octaryn_server_tick(&frame);
    fprintf(s_log, "tick_after_reinitialize=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 7;
    }

    octaryn_chunk_column_snapshot_column chunk_columns[1] = {0};
    const uint32_t chunk_block_capacity = 600000u;
    octaryn_chunk_column_snapshot_block* chunk_blocks =
        (octaryn_chunk_column_snapshot_block*)calloc(
            chunk_block_capacity,
            sizeof(octaryn_chunk_column_snapshot_block));
    if (chunk_blocks == NULL) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 18;
    }

    octaryn_chunk_column_request_frame chunk_request = {0};
    chunk_request.version = 1u;
    chunk_request.size = OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE;
    chunk_request.center_chunk_x = 0;
    chunk_request.center_chunk_z = 0;
    chunk_request.radius = 0u;
    chunk_request.column_capacity = 1u;
    chunk_request.block_capacity = chunk_block_capacity;
    chunk_request.columns_address = (uint64_t)(uintptr_t)chunk_columns;
    chunk_request.blocks_address = (uint64_t)(uintptr_t)chunk_blocks;
    result = octaryn_server_request_chunk_columns(&chunk_request);
    fprintf(s_log, "request_chunk_columns=%d\n", result);
    fprintf(s_log, "request_chunk_columns_columns=%u\n", chunk_request.column_count);
    fprintf(s_log, "request_chunk_columns_blocks=%u\n", chunk_request.block_count);
    if (result != 0 || chunk_request.column_count != 1u || chunk_request.block_count != 0u) {
        free(chunk_blocks);
        octaryn_server_shutdown();
        fclose(s_log);
        return 19;
    }
    free(chunk_blocks);

    octaryn_client_command_frame command_frame = {0};
    command_frame.version = 1u;
    command_frame.size = OCTARYN_CLIENT_COMMAND_FRAME_SIZE;
    command_frame.tick_id = 1u;
    result = octaryn_server_submit_client_commands(&command_frame);
    fprintf(s_log, "submit_client_commands=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 8;
    }

    octaryn_host_command block_commands[1] = {0};
    block_commands[0].version = 1u;
    block_commands[0].size = OCTARYN_HOST_COMMAND_SIZE;
    block_commands[0].kind = 1u;
    block_commands[0].flags = 1u;
    block_commands[0].request_id = 2u;
    block_commands[0].a = 0;
    block_commands[0].b = 34;
    block_commands[0].c = 0;
    block_commands[0].d = 5;

    command_frame.command_count = 1u;
    command_frame.tick_id = 2u;
    command_frame.commands_address = (uint64_t)(uintptr_t)block_commands;
    result = octaryn_server_submit_client_commands(&command_frame);
    fprintf(s_log, "submit_client_commands_set_block_array=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 9;
    }

    result = octaryn_server_tick(&frame);
    fprintf(s_log, "tick_after_submit=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 10;
    }

    block_commands[0].size = 0u;
    command_frame.tick_id = 3u;
    result = octaryn_server_submit_client_commands(&command_frame);
    fprintf(s_log, "submit_client_commands_invalid=%d\n", result);
    if (result != -1) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 11;
    }

    octaryn_replication_change changes[4] = {0};
    octaryn_server_snapshot_header snapshot = {0};
    snapshot.version = 1u;
    snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
    snapshot.change_count = 4u;
    snapshot.tick_id = 1u;
    snapshot.changes_address = (uint64_t)(uintptr_t)changes;
    result = octaryn_server_drain_server_snapshots(&snapshot);
    fprintf(s_log, "drain_server_snapshots=%d\n", result);
    fprintf(s_log, "drain_server_snapshots_block_changes=%u\n", snapshot.change_count);
    if (result != 0 || snapshot.change_count != 1u) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 12;
    }

    result = octaryn_server_drain_server_snapshots(&snapshot);
    fprintf(s_log, "drain_server_snapshots_empty=%d\n", result);
    if (result != 0 || snapshot.change_count != 0u) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 13;
    }

    block_commands[0].size = OCTARYN_HOST_COMMAND_SIZE;
    block_commands[0].request_id = 3u;
    block_commands[0].d = 0;
    command_frame.tick_id = 4u;
    result = octaryn_server_submit_client_commands(&command_frame);
    fprintf(s_log, "submit_client_commands_break_block_array=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 14;
    }

    result = octaryn_server_tick(&frame);
    fprintf(s_log, "tick_after_break_submit=%d\n", result);
    if (result != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 15;
    }

    snapshot.change_count = 4u;
    result = octaryn_server_drain_server_snapshots(&snapshot);
    fprintf(s_log, "drain_server_snapshots_after_break=%d\n", result);
    fprintf(s_log, "drain_server_snapshots_break_changes=%u\n", snapshot.change_count);
    if (result != 0 || snapshot.change_count != 1u) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 16;
    }

    octaryn_chunk_column_snapshot_block* chunk_blocks_after_break =
        (octaryn_chunk_column_snapshot_block*)calloc(
            chunk_block_capacity,
            sizeof(octaryn_chunk_column_snapshot_block));
    if (chunk_blocks_after_break == NULL) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 20;
    }

    octaryn_chunk_column_snapshot_column chunk_columns_after_break[1] = {0};
    octaryn_chunk_column_request_frame chunk_request_after_break = {0};
    chunk_request_after_break.version = 1u;
    chunk_request_after_break.size = OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE;
    chunk_request_after_break.center_chunk_x = 0;
    chunk_request_after_break.center_chunk_z = 0;
    chunk_request_after_break.radius = 0u;
    chunk_request_after_break.column_capacity = 1u;
    chunk_request_after_break.block_capacity = chunk_block_capacity;
    chunk_request_after_break.columns_address = (uint64_t)(uintptr_t)chunk_columns_after_break;
    chunk_request_after_break.blocks_address = (uint64_t)(uintptr_t)chunk_blocks_after_break;
    result = octaryn_server_request_chunk_columns(&chunk_request_after_break);
    fprintf(s_log, "request_chunk_columns_after_break=%d\n", result);
    fprintf(s_log, "request_chunk_columns_after_break_blocks=%u\n", chunk_request_after_break.block_count);
    int resurrected_block = 0;
    for (uint32_t index = 0u; index < chunk_request_after_break.block_count; ++index) {
        const octaryn_chunk_column_snapshot_block block = chunk_blocks_after_break[index];
        if (block.x == 2 && block.y == 3 && block.z == 4 && block.block != 0u) {
            resurrected_block = 1;
            break;
        }
    }
    fprintf(s_log, "request_chunk_columns_after_break_resurrected=%d\n", resurrected_block);
    free(chunk_blocks_after_break);
    if (result != 0 || chunk_request_after_break.column_count != 1u || resurrected_block != 0) {
        octaryn_server_shutdown();
        fclose(s_log);
        return 21;
    }

    octaryn_server_shutdown();
    fprintf(s_log, "shutdown=0\n");
    fclose(s_log);

    return result == 0 ? 0 : 17;
}
