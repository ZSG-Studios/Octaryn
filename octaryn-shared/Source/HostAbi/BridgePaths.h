#pragma once

#include <hostfxr.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <wchar.h>
#define OCTARYN_BRIDGE_PATH_CAPACITY 32768u
#define octaryn_bridge_path_length wcslen
#else
#include <dlfcn.h>
#define OCTARYN_BRIDGE_PATH_CAPACITY 4096u
#define octaryn_bridge_path_length strlen
#endif

static int octaryn_copy_bridge_path(char_t* output, size_t capacity, const char_t* path)
{
    const size_t length = octaryn_bridge_path_length(path);
    if (length >= capacity) {
        return 0;
    }
    memcpy(output, path, (length + 1u) * sizeof(char_t));
    return 1;
}

static int octaryn_resolve_bridge_path(
    char_t* output, size_t capacity, const void* module_address,
    const char_t* configured_path, const char_t* environment_name)
{
    if (output == NULL || capacity == 0 || configured_path == NULL || configured_path[0] == 0) {
        return 0;
    }

#if defined(_WIN32)
    const DWORD environment_length = GetEnvironmentVariableW(environment_name, output, (DWORD)capacity);
    if (environment_length != 0) {
        return environment_length < capacity;
    }
    if (configured_path[0] == L'/' || configured_path[0] == L'\\' ||
        (configured_path[0] != 0 && configured_path[1] == L':')) {
        return octaryn_copy_bridge_path(output, capacity, configured_path);
    }

    HMODULE module = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)module_address, &module)) {
        return 0;
    }
    const DWORD path_length = GetModuleFileNameW(module, output, (DWORD)capacity);
    if (path_length == 0 || path_length >= capacity) {
        return 0;
    }
#else
    const char* environment_path = getenv(environment_name);
    if (environment_path != NULL && environment_path[0] != '\0') {
        return octaryn_copy_bridge_path(output, capacity, environment_path);
    }
    if (configured_path[0] == '/') {
        return octaryn_copy_bridge_path(output, capacity, configured_path);
    }

    Dl_info module;
    if (dladdr(module_address, &module) == 0 || module.dli_fname == NULL ||
        !octaryn_copy_bridge_path(output, capacity, module.dli_fname)) {
        return 0;
    }
#endif

    size_t directory_length = octaryn_bridge_path_length(output);
    while (directory_length > 0 && output[directory_length - 1u] != '/' &&
           output[directory_length - 1u] != '\\') {
        --directory_length;
    }
    if (directory_length == 0) {
        return 0;
    }
    return octaryn_copy_bridge_path(output + directory_length,
                                   capacity - directory_length, configured_path);
}
