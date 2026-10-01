#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#define OCTARYN_ABI_BUILD
#include "HostExports.h"
#include "BridgePaths.h"
#include "octaryn_native_crash_diagnostics.h"

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <stddef.h>
#include <string.h>

#include "DotNetHostLoader.h"

#if defined(_WIN32)
#define OCTARYN_NATIVE_TEXT_IMPL(value) L##value
#define OCTARYN_NATIVE_TEXT(value) OCTARYN_NATIVE_TEXT_IMPL(value)
#else
#define OCTARYN_NATIVE_TEXT(value) value
#endif

#ifndef OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH
#define OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH "Octaryn.Client.dll"
#endif
#ifndef OCTARYN_CLIENT_RUNTIME_CONFIG_PATH
#define OCTARYN_CLIENT_RUNTIME_CONFIG_PATH "Octaryn.Client.runtimeconfig.json"
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
typedef void (OCTARYN_ABI_CALL* octaryn_client_shutdown_fn)(void);
typedef int (OCTARYN_ABI_CALL* octaryn_client_remote_start_fn)(const char* endpoint_utf8, const char* runtime_directory_utf8);
typedef void (OCTARYN_ABI_CALL* octaryn_client_remote_stop_fn)(void);
typedef int (OCTARYN_ABI_CALL* octaryn_client_remote_is_running_fn)(void);
typedef int (OCTARYN_ABI_CALL* octaryn_client_remote_status_fn)(char* buffer, int capacity);
typedef int (OCTARYN_ABI_CALL* octaryn_client_remote_poll_module_event_fn)(
    uint64_t* event_id, uint64_t* kind, uint64_t* payload1, uint64_t* payload2);

typedef int (OCTARYN_ABI_CALL* remote_poll_pose_fn)(octaryn_remote_pose*);
typedef int (OCTARYN_ABI_CALL* remote_submit_commands_fn)(const octaryn_remote_command*, int, int);
static remote_poll_pose_fn s_remote_poll_pose;
typedef int (OCTARYN_ABI_CALL* remote_copy_world_items_fn)(uint64_t*, void*, int, int);
static remote_copy_world_items_fn s_remote_copy_world_items;
static remote_submit_commands_fn s_remote_submit_commands;

typedef int (OCTARYN_ABI_CALL* remote_submit_intent_fn)(uint8_t, const char*, int);
typedef int (OCTARYN_ABI_CALL* remote_poll_action_ack_fn)(uint64_t*, uint64_t*);
static remote_submit_intent_fn s_remote_submit_intent;
static remote_poll_action_ack_fn s_remote_poll_action_ack;

static octaryn_client_initialize_fn s_initialize;
static octaryn_client_tick_fn s_tick;
static octaryn_client_apply_server_snapshot_fn s_apply_server_snapshot;
static octaryn_client_drain_presentation_updates_fn s_drain_presentation_updates;
static octaryn_client_shutdown_fn s_shutdown;
static octaryn_client_remote_start_fn s_remote_start;
static octaryn_client_remote_start_fn s_remote_start_async;
static octaryn_client_remote_stop_fn s_remote_stop;
static octaryn_client_remote_is_running_fn s_remote_is_running;
static octaryn_client_remote_status_fn s_remote_status;
static octaryn_client_remote_poll_module_event_fn s_remote_poll_module_event;
static int s_load_result;
static char_t s_managed_assembly_path[OCTARYN_BRIDGE_PATH_CAPACITY];

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
        s_managed_assembly_path,
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
        s_shutdown != NULL &&
        s_remote_start != NULL && s_remote_start_async != NULL &&
        s_remote_stop != NULL &&
        s_remote_is_running != NULL &&
        s_remote_status != NULL &&
        s_remote_poll_module_event != NULL && s_remote_poll_pose != NULL && s_remote_copy_world_items != NULL && s_remote_submit_commands != NULL && s_remote_submit_intent != NULL && s_remote_poll_action_ack != NULL) {
        return 0;
    }

    if (s_load_result != 0) {
        return s_load_result;
    }

    char_t runtime_config_path[OCTARYN_BRIDGE_PATH_CAPACITY];
    if (!octaryn_resolve_bridge_path(s_managed_assembly_path, OCTARYN_BRIDGE_PATH_CAPACITY,
            &s_load_result, OCTARYN_NATIVE_TEXT(OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH),
            OCTARYN_NATIVE_TEXT("OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH")) ||
        !octaryn_resolve_bridge_path(runtime_config_path, OCTARYN_BRIDGE_PATH_CAPACITY,
            &s_load_result, OCTARYN_NATIVE_TEXT(OCTARYN_CLIENT_RUNTIME_CONFIG_PATH),
            OCTARYN_NATIVE_TEXT("OCTARYN_CLIENT_RUNTIME_CONFIG_PATH"))) {
        s_load_result = OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    void* hostfxr = octaryn_open_hostfxr();
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
        runtime_config_path,
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
        OCTARYN_NATIVE_TEXT("Shutdown"),
        (void**)&s_shutdown);
    if (result < 0 || s_shutdown == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteStart"),
        (void**)&s_remote_start);
    if (result < 0 || s_remote_start == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteStop"),
        (void**)&s_remote_stop);
    if (result < 0 || s_remote_stop == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteIsRunning"),
        (void**)&s_remote_is_running);
    if (result < 0 || s_remote_is_running == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteStatus"),
        (void**)&s_remote_status);
    if (result < 0 || s_remote_status == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(
        load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemotePollModuleEvent"),
        (void**)&s_remote_poll_module_event);
    if (result < 0 || s_remote_poll_module_event == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }

    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemotePollPose"), (void**)&s_remote_poll_pose);
    if (result < 0 || s_remote_poll_pose == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }
    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteCopyWorldItems"), (void**)&s_remote_copy_world_items);
    if (result < 0 || s_remote_copy_world_items == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }
    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteSubmitCommands"), (void**)&s_remote_submit_commands);
    if (result < 0 || s_remote_submit_commands == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }
    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteStartAsync"), (void**)&s_remote_start_async);
    if (result < 0 || s_remote_start_async == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }
    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemotePollActionAck"), (void**)&s_remote_poll_action_ack);
    if (result < 0 || s_remote_poll_action_ack == NULL) {
        s_load_result = result < 0 ? result : OCTARYN_CLIENT_BRIDGE_LOAD_FAILED;
        return s_load_result;
    }
    result = octaryn_resolve_managed_method(load_assembly,
        OCTARYN_NATIVE_TEXT("Octaryn.Client.HostBridge.HostExports, Octaryn.Client"),
        OCTARYN_NATIVE_TEXT("RemoteSubmitIntent"), (void**)&s_remote_submit_intent);
    if (result < 0 || s_remote_submit_intent == NULL) {
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

void OCTARYN_ABI_CALL octaryn_client_shutdown(void)
{
    if (s_shutdown != NULL) {
        s_shutdown();
    }
}

int OCTARYN_ABI_CALL octaryn_client_remote_start(const char* endpoint_utf8, const char* runtime_directory_utf8)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_remote_start(endpoint_utf8, runtime_directory_utf8);
}

void OCTARYN_ABI_CALL octaryn_client_remote_stop(void)
{
    if (s_remote_stop != NULL) {
        s_remote_stop();
    }
}

int OCTARYN_ABI_CALL octaryn_client_remote_is_running(void)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return 0;
    }

    return s_remote_is_running();
}

int OCTARYN_ABI_CALL octaryn_client_remote_status(char* buffer, int capacity)
{
    int result = octaryn_client_load_managed_exports();
    if (result < 0) {
        return result;
    }

    return s_remote_status(buffer, capacity);
}

int OCTARYN_ABI_CALL octaryn_client_remote_poll_module_event(
    uint64_t* event_id, uint64_t* kind, uint64_t* payload1, uint64_t* payload2)
{
    if (s_remote_poll_module_event == NULL) {
        return 0;
    }

    return s_remote_poll_module_event(event_id, kind, payload1, payload2);
}

int OCTARYN_ABI_CALL octaryn_client_remote_copy_world_items(uint64_t* revision, void* output, int capacity, int stride) {
    return s_remote_copy_world_items ? s_remote_copy_world_items(revision, output, capacity, stride) : -2;
}

int OCTARYN_ABI_CALL octaryn_client_remote_poll_pose(octaryn_remote_pose* pose) {
    return s_remote_poll_pose != NULL ? s_remote_poll_pose(pose) : 0;
}
int OCTARYN_ABI_CALL octaryn_client_remote_submit_commands(const octaryn_remote_command* commands, int count, int stride) {
    return s_remote_submit_commands != NULL ? s_remote_submit_commands(commands, count, stride) : -1;
}

int OCTARYN_ABI_CALL octaryn_client_remote_start_async(const char* endpoint, const char* directory) {
    int result = octaryn_client_load_managed_exports();
    return result < 0 ? result : s_remote_start_async(endpoint, directory);
}

int OCTARYN_ABI_CALL octaryn_client_remote_submit_intent(uint8_t kind, const char* payload, int length) {
    return s_remote_submit_intent != NULL ? s_remote_submit_intent(kind, payload, length) : -1;
}
int OCTARYN_ABI_CALL octaryn_client_remote_poll_action_ack(uint64_t* epoch, uint64_t* sequence) {
    return s_remote_poll_action_ack != NULL ? s_remote_poll_action_ack(epoch, sequence) : 0;
}
