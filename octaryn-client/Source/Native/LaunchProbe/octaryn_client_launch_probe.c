#include "octaryn_client_host_exports.h"
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

static int32_t octaryn_probe_unpack_low(uint64_t value)
{
    return (int32_t)(uint32_t)value;
}

static int32_t octaryn_probe_unpack_high(uint64_t value)
{
    return (int32_t)(uint32_t)(value >> 32u);
}

static int octaryn_probe_drain_mesh_upload(
    octaryn_client_chunk_mesh_upload_record* upload,
    uint64_t* opaque_faces,
    uint32_t* opaque_faces_written)
{
    uint32_t upload_written = 0u;
    uint32_t transparent_faces_written = 0u;
    uint32_t sprite_vertices_written = 0u;
    uint64_t transparent_faces[16] = {0};
    uint32_t sprite_vertices[16] = {0};
    return octaryn_client_drain_chunk_mesh_uploads(
        upload,
        1u,
        &upload_written,
        opaque_faces,
        16u,
        opaque_faces_written,
        transparent_faces,
        16u,
        &transparent_faces_written,
        sprite_vertices,
        16u,
        &sprite_vertices_written);
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

    octaryn_client_chunk_mesh_upload_record mesh_upload = {0};
    uint64_t opaque_faces[16] = {0};
    uint32_t opaque_faces_written = 0u;
    result = octaryn_probe_drain_mesh_upload(&mesh_upload, opaque_faces, &opaque_faces_written);
    fprintf(s_log, "drain_chunk_mesh_uploads_before_initialize=%d\n", result);
    if (result != -1) {
        fclose(s_log);
        return 15;
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
    fprintf(s_log, "drain_presentation_updates=%d count=%u x=%d y=%d z=%d block=%u\n",
        result,
        written,
        octaryn_probe_unpack_low(presentation_changes[0].payload0),
        octaryn_probe_unpack_high(presentation_changes[0].payload0),
        octaryn_probe_unpack_low(presentation_changes[0].payload1),
        (uint32_t)(presentation_changes[0].payload1 >> 32u));
    if (result != 0 ||
        written != 1u ||
        presentation_changes[0].change_kind != 1u ||
        octaryn_probe_unpack_low(presentation_changes[0].payload0) != -4 ||
        octaryn_probe_unpack_high(presentation_changes[0].payload0) != 5 ||
        octaryn_probe_unpack_low(presentation_changes[0].payload1) != 6 ||
        (uint32_t)(presentation_changes[0].payload1 >> 32u) != 7u) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 13;
    }

    mesh_upload = (octaryn_client_chunk_mesh_upload_record){0};
    opaque_faces_written = 0u;
    result = octaryn_probe_drain_mesh_upload(&mesh_upload, opaque_faces, &opaque_faces_written);
    fprintf(s_log,
        "drain_chunk_mesh_uploads=%d chunk_count=%u chunk=(%d,%d,%d) opaque_faces=%u transparent_faces=%u sprite_vertices=%u fluid_blocks=%u opaque_written=%u bytes=%llu\n",
        result,
        1u,
        mesh_upload.chunk_x,
        mesh_upload.chunk_y,
        mesh_upload.chunk_z,
        mesh_upload.opaque_face_count,
        mesh_upload.transparent_face_count,
        mesh_upload.sprite_vertex_count,
        mesh_upload.fluid_block_count,
        opaque_faces_written,
        (unsigned long long)mesh_upload.opaque_byte_count);
    if (result != 0 ||
        mesh_upload.version != 1u ||
        mesh_upload.size != OCTARYN_CLIENT_CHUNK_MESH_UPLOAD_RECORD_SIZE ||
        mesh_upload.chunk_x != -1 ||
        mesh_upload.chunk_y != 0 ||
        mesh_upload.chunk_z != 0 ||
        mesh_upload.opaque_face_count != 6u ||
        mesh_upload.transparent_face_count != 0u ||
        mesh_upload.sprite_vertex_count != 0u ||
        mesh_upload.fluid_block_count != 0u ||
        mesh_upload.opaque_face_offset != 0u ||
        mesh_upload.opaque_byte_count != 48u ||
        opaque_faces_written != 6u ||
        opaque_faces[0] == 0u) {
        octaryn_client_shutdown();
        fclose(s_log);
        return 16;
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
    if (result != -2) {
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
