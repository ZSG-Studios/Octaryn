#include "app/managed/managed_host.h"

#include "app/managed/managed_abi.h"
#include "app/managed/managed_command_queue.h"
#include "core/log.h"

#include <SDL3/SDL.h>

#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

#include <array>
#include <cstddef>
#include <cstdint>
#endif

namespace {

#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)

using managed_initialize_fn = int (*)(const oct_managed_native_api_t*);
using managed_tick_fn = void (*)(const oct_managed_frame_snapshot_t*);
using managed_shutdown_fn = void (*)();

struct managed_host_state_t
{
    SDL_SharedObject* hostfxr_library = nullptr;
    hostfxr_initialize_for_runtime_config_fn initialize_for_runtime_config = nullptr;
    hostfxr_get_runtime_delegate_fn get_runtime_delegate = nullptr;
    hostfxr_close_fn close = nullptr;
    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;
    managed_initialize_fn initialize = nullptr;
    managed_tick_fn tick = nullptr;
    managed_shutdown_fn shutdown = nullptr;
    oct_managed_native_api_t native_api = {};
    bool active = false;
};

managed_host_state_t g_managed_host = {};

void managed_hostfxr_error_writer(const char_t* message)
{
    if (message != nullptr)
    {
        oct_log_errorf(".NET hostfxr: %s", message);
    }
}

bool managed_build_runtime_path(char* output, size_t output_size, const char* leaf)
{
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr || output == nullptr || output_size == 0u)
    {
        return false;
    }
    const int written = SDL_snprintf(output, output_size, "%sdotnet/client/%s", base_path, leaf);
    return written > 0 && static_cast<size_t>(written) < output_size;
}

template <typename Fn>
bool managed_load_hostfxr_export(SDL_SharedObject* library, const char* name, Fn* out_function)
{
    *out_function = reinterpret_cast<Fn>(SDL_LoadFunction(library, name));
    if (*out_function == nullptr)
    {
        oct_log_errorf("Failed to load .NET hostfxr export %s: %s", name, SDL_GetError());
        return false;
    }
    return true;
}

bool managed_load_hostfxr(managed_host_state_t* host)
{
    std::array<char_t, 4096> hostfxr_path = {};
    size_t hostfxr_path_size = hostfxr_path.size();
    const int hostfxr_path_result = get_hostfxr_path(hostfxr_path.data(), &hostfxr_path_size, nullptr);
    if (hostfxr_path_result != 0)
    {
        oct_log_errorf("Failed to locate .NET hostfxr library: 0x%x", hostfxr_path_result);
        return false;
    }

    host->hostfxr_library = SDL_LoadObject(hostfxr_path.data());
    if (host->hostfxr_library == nullptr)
    {
        oct_log_errorf("Failed to load .NET hostfxr library %s: %s", hostfxr_path.data(), SDL_GetError());
        return false;
    }

    hostfxr_set_error_writer_fn set_error_writer = nullptr;
    if (!managed_load_hostfxr_export(host->hostfxr_library, "hostfxr_set_error_writer", &set_error_writer) ||
        !managed_load_hostfxr_export(host->hostfxr_library,
                                     "hostfxr_initialize_for_runtime_config",
                                     &host->initialize_for_runtime_config) ||
        !managed_load_hostfxr_export(host->hostfxr_library, "hostfxr_get_runtime_delegate", &host->get_runtime_delegate) ||
        !managed_load_hostfxr_export(host->hostfxr_library, "hostfxr_close", &host->close))
    {
        return false;
    }

    set_error_writer(managed_hostfxr_error_writer);
    return true;
}

bool managed_load_runtime_delegate(managed_host_state_t* host, const char* runtime_config_path)
{
    hostfxr_handle runtime_handle = nullptr;
    const int32_t init_result = host->initialize_for_runtime_config(runtime_config_path, nullptr, &runtime_handle);
    if (init_result < 0 || runtime_handle == nullptr)
    {
        oct_log_errorf("Failed to initialize .NET runtime config %s: 0x%x", runtime_config_path, init_result);
        return false;
    }

    void* load_assembly = nullptr;
    const int32_t delegate_result =
        host->get_runtime_delegate(runtime_handle, hdt_load_assembly_and_get_function_pointer, &load_assembly);
    host->close(runtime_handle);
    if (delegate_result < 0 || load_assembly == nullptr)
    {
        oct_log_errorf("Failed to get .NET assembly loader delegate: 0x%x", delegate_result);
        return false;
    }

    host->load_assembly_and_get_function_pointer =
        reinterpret_cast<load_assembly_and_get_function_pointer_fn>(load_assembly);
    return true;
}

template <typename Fn>
bool managed_load_game_export(managed_host_state_t* host, const char* assembly_path, const char* method_name, Fn* out_function)
{
    void* function = nullptr;
    constexpr const char_t* kGameExportType = "Octaryn.Client.ClientHostExports, Octaryn.Client";
    const int32_t result = host->load_assembly_and_get_function_pointer(
        assembly_path,
        kGameExportType,
        method_name,
        UNMANAGEDCALLERSONLY_METHOD,
        nullptr,
        &function);
    if (result < 0 || function == nullptr)
    {
        oct_log_errorf("Failed to load managed game export %s: 0x%x", method_name, result);
        return false;
    }

    *out_function = reinterpret_cast<Fn>(function);
    return true;
}

void managed_host_reset(managed_host_state_t* host)
{
    if (host->hostfxr_library != nullptr)
    {
        SDL_UnloadObject(host->hostfxr_library);
    }
    *host = {};
}

#endif

} // namespace

bool app_managed_host_startup(void)
{
#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)
    app_managed_command_queue_init();

    if (SDL_getenv("OCTARYN_MANAGED_DISABLE") != nullptr)
    {
        oct_log_infof("Managed game layer disabled by OCTARYN_MANAGED_DISABLE");
        return true;
    }

    std::array<char, 4096> runtime_config_path = {};
    std::array<char, 4096> assembly_path = {};
    if (!managed_build_runtime_path(runtime_config_path.data(), runtime_config_path.size(), "Octaryn.Client.runtimeconfig.json") ||
        !managed_build_runtime_path(assembly_path.data(), assembly_path.size(), "Octaryn.Client.dll"))
    {
        oct_log_errorf("Failed to build managed game paths");
        return false;
    }
    if (!SDL_GetPathInfo(runtime_config_path.data(), nullptr) || !SDL_GetPathInfo(assembly_path.data(), nullptr))
    {
        oct_log_warnf("Managed game layer is not published at %s; native runtime will continue", assembly_path.data());
        return true;
    }

    if (!managed_load_hostfxr(&g_managed_host) ||
        !managed_load_runtime_delegate(&g_managed_host, runtime_config_path.data()) ||
        !managed_load_game_export(&g_managed_host, assembly_path.data(), "Initialize", &g_managed_host.initialize) ||
        !managed_load_game_export(&g_managed_host, assembly_path.data(), "Tick", &g_managed_host.tick) ||
        !managed_load_game_export(&g_managed_host, assembly_path.data(), "Shutdown", &g_managed_host.shutdown))
    {
        managed_host_reset(&g_managed_host);
        return false;
    }

    g_managed_host.native_api = {
        OCT_MANAGED_NATIVE_API_VERSION,
        static_cast<std::uint32_t>(sizeof(oct_managed_native_api_t)),
        app_managed_command_queue_enqueue,
    };

    const int initialize_result = g_managed_host.initialize(&g_managed_host.native_api);
    if (initialize_result != 0)
    {
        oct_log_errorf("Managed game Initialize failed with code %d", initialize_result);
        managed_host_reset(&g_managed_host);
        return false;
    }

    g_managed_host.active = true;
    oct_log_infof("Managed frame snapshot ABI version=%u size=%u",
                  OCT_MANAGED_FRAME_SNAPSHOT_VERSION,
                  static_cast<unsigned int>(sizeof(oct_managed_frame_snapshot_t)));
    oct_log_infof("Managed game layer loaded from %s", assembly_path.data());
    return true;
#else
    return true;
#endif
}

void app_managed_host_tick(const oct_managed_frame_snapshot_t* frame_snapshot)
{
#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)
    if (g_managed_host.active && g_managed_host.tick != nullptr && frame_snapshot != nullptr)
    {
        g_managed_host.tick(frame_snapshot);
    }
#else
    (void) frame_snapshot;
#endif
}

void app_managed_host_drain_commands(void)
{
#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)
    app_managed_command_queue_drain();
#endif
}

void app_managed_host_shutdown(void)
{
#if defined(OCTARYN_ENABLE_MANAGED_DOTNET)
    if (g_managed_host.active && g_managed_host.shutdown != nullptr)
    {
        g_managed_host.shutdown();
    }
    managed_host_reset(&g_managed_host);
    app_managed_command_queue_shutdown();
#endif
}
