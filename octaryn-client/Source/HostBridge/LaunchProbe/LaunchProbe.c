#include "HostExports.h"
#include "octaryn_native_crash_diagnostics.h"

#include <stdint.h>
#include <stdio.h>

static FILE* s_log;

static int OCTARYN_ABI_CALL octaryn_probe_enqueue_command(octaryn_host_command* command)
{
    if (s_log != NULL && command != NULL) {
        fprintf(s_log, "enqueue_command kind=%u request=%llu\n",
            command->kind,
            (unsigned long long)command->request_id);
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
    frame.timing.version = 1u;
    frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
    frame.timing.frame_index = 1u;
    frame.timing.delta_seconds = 1.0 / 60.0;
    return frame;
}

static uint64_t octaryn_probe_pack_signed_pair(int32_t a, int32_t b)
{
    return (uint32_t)a | ((uint64_t)(uint32_t)b << 32u);
}

static uint64_t octaryn_probe_pack_block(int32_t z, uint16_t block)
{
    return (uint32_t)z | ((uint64_t)block << 32u);
}

int main(void)
{
    s_log = fopen(OCTARYN_CLIENT_LAUNCH_PROBE_LOG_PATH, "w");
    if (s_log == NULL) {
        return 2;
    }

    octaryn_native_crash_diagnostics_init("client-launch-probe");
    fprintf(s_log, "crash_marker=%s\n", octaryn_native_crash_diagnostics_marker_path());

    octaryn_client_native_host_api api = {0};
    api.version = 1u;
    api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
    api.enqueue_command = octaryn_probe_enqueue_command;

    octaryn_host_frame_snapshot frame = octaryn_probe_frame();
    octaryn_replication_change changes[1] = {0};
    changes[0].version = 1u;
    changes[0].size = OCTARYN_REPLICATION_CHANGE_SIZE;
    changes[0].change_kind = 1u;
    changes[0].replication_id = 1u;
    changes[0].payload0 = octaryn_probe_pack_signed_pair(-4, 5);
    changes[0].payload1 = octaryn_probe_pack_block(6, 7u);

    octaryn_server_snapshot_header snapshot = {0};
    snapshot.version = 1u;
    snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
    snapshot.change_count = 1u;
    snapshot.tick_id = 1u;
    snapshot.changes_address = (uint64_t)(uintptr_t)changes;

    int result = octaryn_client_tick(&frame);
    fprintf(s_log, "tick_before_initialize=%d\n", result);
    if (result != -1) {
        fclose(s_log);
        return 3;
    }

    result = octaryn_client_apply_server_snapshot(&snapshot);
    fprintf(s_log, "apply_server_snapshot_before_initialize=%d\n", result);
    if (result != -1) {
        fclose(s_log);
        return 4;
    }

    uint32_t written = 0u;
    result = octaryn_client_drain_presentation_updates(changes, 1u, &written);
    fprintf(s_log, "drain_presentation_updates_before_initialize=%d\n", result);
    if (result != -1) {
        fclose(s_log);
        return 12;
    }

    result = octaryn_client_initialize(&api);
    fprintf(s_log, "initialize=%d\n", result);
    if (result != 0) {
        fclose(s_log);
        return 5;
    }

    result = octaryn_client_tick(&frame);
    fprintf(s_log, "tick=%d\n", result);
    if (result != 0) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 6;
    }

    result = octaryn_client_apply_server_snapshot(&snapshot);
    fprintf(s_log, "apply_server_snapshot=%d\n", result);
    if (result != 0) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 7;
    }

    octaryn_replication_change presentation_changes[1] = {0};
    written = 0u;
    result = octaryn_client_drain_presentation_updates(presentation_changes, 1u, &written);
    fprintf(s_log, "drain_presentation_updates=%d count=%u\n",
        result,
        written);
    if (result != 0 ||
        written != 0u) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 13;
    }

    written = 1u;
    result = octaryn_client_drain_presentation_updates(presentation_changes, 1u, &written);
    fprintf(s_log, "drain_presentation_updates_empty=%d count=%u\n", result, written);
    if (result != 0 || written != 0u) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 14;
    }

    changes[0].change_kind = 999u;
    result = octaryn_client_apply_server_snapshot(&snapshot);
    fprintf(s_log, "apply_server_snapshot_invalid=%d\n", result);
    if (result != 0) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 8;
    }
    changes[0].change_kind = 1u;

    result = octaryn_client_initialize(&api);
    fprintf(s_log, "reinitialize=%d\n", result);
    if (result != 0) {
        fclose(s_log);
        return 9;
    }

    result = octaryn_client_tick(&frame);
    fprintf(s_log, "tick_after_reinitialize=%d\n", result);
    if (result != 0) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 10;
    }

    octaryn_client_shutdown();
    fprintf(s_log, "shutdown=0\n");
    fclose(s_log);

    return result == 0 ? 0 : 11;
}
