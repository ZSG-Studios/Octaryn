#include "HostExports.h"
#include "octaryn_native_crash_diagnostics.h"

#include <stdint.h>
#include <stdio.h>

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

    octaryn_server_shutdown();
    fprintf(s_log, "shutdown=0\n");
    fclose(s_log);

    return 0;
}
