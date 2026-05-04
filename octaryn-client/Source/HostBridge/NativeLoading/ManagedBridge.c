#define OCTARYN_ABI_BUILD
#include "HostExports.h"
#include "octaryn_native_crash_diagnostics.h"

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define OCTARYN_NATIVE_TEXT_IMPL(value) L##value
#define OCTARYN_NATIVE_TEXT(value) OCTARYN_NATIVE_TEXT_IMPL(value)
#else
#define OCTARYN_NATIVE_TEXT(value) value
#endif

enum {
    OCTARYN_CLIENT_BRIDGE_LOAD_FAILED = -100
};

typedef int (OCTARYN_ABI_CALL* octaryn_client_initialize_fn)(octaryn_client_native_host_api* native_api);
typedef int (OCTARYN_ABI_CALL* octaryn_client_tick_fn)(octaryn_host_frame_snapshot* frame_snapshot);
typedef int (OCTARYN_ABI_CALL* octaryn_client_apply_server_snapshot_fn)(octaryn_server_snapshot_header* snapshot_header);
typedef int (OCTARYN_ABI_CALL* octaryn_client_drain_presentation_updates_fn)(
    octaryn_replication_change* changes,
    uint32_t capacity,
    uint32_t* written);
typedef int (OCTARYN_ABI_CALL* octaryn_client_drain_chunk_mesh_uploads_fn)(
    octaryn_client_chunk_mesh_upload_record* uploads,
    uint32_t upload_capacity,
    uint32_t* upload_written,
    uint64_t* opaque_faces,
    uint32_t opaque_face_capacity,
    uint32_t* opaque_faces_written,
    uint64_t* transparent_faces,
    uint32_t transparent_face_capacity,
    uint32_t* transparent_faces_written,
    uint32_t* sprite_vertices,
    uint32_t sprite_vertex_capacity,
    uint32_t* sprite_vertices_written);
typedef void (OCTARYN_ABI_CALL* octaryn_client_shutdown_fn)(void);

static octaryn_client_initialize_fn s_initialize;
static octaryn_client_tick_fn s_tick;
static octaryn_client_apply_server_snapshot_fn s_apply_server_snapshot;
static octaryn_client_drain_presentation_updates_fn s_drain_presentation_updates;
static octaryn_client_drain_chunk_mesh_uploads_fn s_drain_chunk_mesh_uploads;
static octaryn_client_shutdown_fn s_shutdown;
static int s_load_result;

static void* octaryn_open_library(const char* path)
{
#if defined(_WIN32)
    return (void*)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* octaryn_load_symbol(void* library, const char* symbol)
{
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)library, symbol);
#else
    return dlsym(library, symbol);
#endif
}

static int octaryn_load_hostfxr_symbol(void* library, const char* symbol, void* target, size_t target_size)
{
    void* address = octaryn_load_symbol(library, symbol);
    if (address == NULL) {
        return 0;
    }

    memcpy(target, &address, target_size);
    return 1;
}

static int octaryn_resolve_managed_method(
    load_assembly_and_get_function_pointer_fn load_assembly,
    const char_t* type_name,
    const char_t* method_name,
    void** target)
{
    return load_assembly(
        OCTARYN_NATIVE_TEXT(OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH),
        type_name,
        method_name,
        UNMANAGEDCALLERSONLY_METHOD,
        NULL,
        target);
}

static int octaryn_client_load_managed_exports(void)
{
    octaryn_native_crash_diagnostics_init("octaryn-client-native");

    if (s_initialize != NULL &&
        s_tick != NULL &&
        s_apply_server_snapshot != NULL &&
        s_drain_presentation_updates != NULL &&
        s_drain_chunk_mesh_uploads != NULL &&
        s_shutdown != NULL) {
        return 0;
    }

    if (s_load_result != 0) {
        return s_load_result;
    }

    void* hostfxr = octaryn_open_library(OCTARYN_DOTNET_HOSTFXR_PATH);
    if (hostfxr == NULL) {
        s_load_result = OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    hostfxr_initialize_for_runtime_config_fn initialize_for_runtime_config = NULL;
    hostfxr_get_runtime_delegate_fn get_runtime_delegate = NULL;
    hostfxr_close_fn close_host_context = NULL;

    octaryn_load_hostfxr_symbol(
        hostfxr,
        "hostfxr_initialize_for_runtime_config",
        &initialize_for_runtime_config,
        sizeof(initialize_for_runtime_config));
    octaryn_load_hostfxr_symbol(
        hostfxr,
        "hostfxr_get_runtime_delegate",
        &get_runtime_delegate,
        sizeof(get_runtime_delegate));
    octaryn_load_hostfxr_symbol(
        hostfxr,
        "hostfxr_close",
        &close_host_context,
        sizeof(close_host_context));

    if (initialize_for_runtime_config == NULL || get_runtime_delegate == NULL || close_host_context == NULL) {
        s_load_result = OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    hostfxr_handle host_context = NULL;
    int result = initialize_for_runtime_config(
        OCTARYN_NATIVE_TEXT(OCTARYN_CLIENT_RUNTIME_CONFIG_PATH),
        NULL,
        &host_context);
    if (result < 0 || host_context == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    load_assembly_and_get_function_pointer_fn load_assembly = NULL;
    result = get_runtime_delegate(
        host_context,
        hdt_load_assembly_and_get_function_pointer,
        (void**)&load_assembly);
    close_host_context(host_context);
    if (result < 0 || load_assembly == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("Initialize"),
        (void**)&s_initialize);
    if (result < 0 || s_initialize == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("Tick"),
        (void**)&s_tick);
    if (result < 0 || s_tick == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("ApplyServerSnapshot"),
        (void**)&s_apply_server_snapshot);
    if (result < 0 || s_apply_server_snapshot == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("DrainPresentationUpdates"),
        (void**)&s_drain_presentation_updates);
    if (result < 0 || s_drain_presentation_updates == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("DrainChunkMeshUploads"),
        (void**)&s_drain_chunk_mesh_uploads);
    if (result < 0 || s_drain_chunk_mesh_uploads == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("Shutdown"),
        (void**)&s_shutdown);
    if (result < 0 || s_shutdown == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    return 0;
}

int OCTARYN_ABI_CALL octaryn_client_initialize(octaryn_client_native_host_api* native_api)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_initialize(native_api);
}

int OCTARYN_ABI_CALL octaryn_client_tick(octaryn_host_frame_snapshot* frame_snapshot)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_tick(frame_snapshot);
}

int OCTARYN_ABI_CALL octaryn_client_apply_server_snapshot(octaryn_server_snapshot_header* snapshot_header)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_apply_server_snapshot(snapshot_header);
}

int OCTARYN_ABI_CALL octaryn_client_drain_presentation_updates(
    octaryn_replication_change* changes,
    uint32_t capacity,
    uint32_t* written)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_drain_presentation_updates(changes, capacity, written);
}

int OCTARYN_ABI_CALL octaryn_client_drain_chunk_mesh_uploads(
    octaryn_client_chunk_mesh_upload_record* uploads,
    uint32_t upload_capacity,
    uint32_t* upload_written,
    uint64_t* opaque_faces,
    uint32_t opaque_face_capacity,
    uint32_t* opaque_faces_written,
    uint64_t* transparent_faces,
    uint32_t transparent_face_capacity,
    uint32_t* transparent_faces_written,
    uint32_t* sprite_vertices,
    uint32_t sprite_vertex_capacity,
    uint32_t* sprite_vertices_written)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_drain_chunk_mesh_uploads(
        uploads,
        upload_capacity,
        upload_written,
        opaque_faces,
        opaque_face_capacity,
        opaque_faces_written,
        transparent_faces,
        transparent_face_capacity,
        transparent_faces_written,
        sprite_vertices,
        sprite_vertex_capacity,
        sprite_vertices_written);
}

void OCTARYN_ABI_CALL octaryn_client_shutdown(void)
{
    if (s_shutdown != NULL) {
        s_shutdown();
    }
}
