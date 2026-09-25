#pragma once

#include <nethost.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void* octaryn_open_hostfxr(void)
{
    struct get_hostfxr_parameters parameters = {sizeof(struct get_hostfxr_parameters), NULL, NULL};
#if defined(_WIN32)
    wchar_t* dotnet_root = NULL;
    size_t dotnet_root_size = 0;
    if (_wdupenv_s(&dotnet_root, &dotnet_root_size, L"DOTNET_ROOT") != 0) {
        free(dotnet_root);
        return NULL;
    }
    parameters.dotnet_root = dotnet_root;
#else
    parameters.dotnet_root = getenv("DOTNET_ROOT");
#endif
    if (parameters.dotnet_root != NULL && parameters.dotnet_root[0] == 0) {
        parameters.dotnet_root = NULL;
    }

    size_t path_size = 0;
    char_t* path = NULL;
    void* library = NULL;
    const int result = get_hostfxr_path(NULL, &path_size, &parameters);
    if ((uint32_t)result != UINT32_C(0x80008098) || path_size == 0 ||
        path_size > SIZE_MAX / sizeof(char_t)) {
        goto cleanup;
    }
    path = (char_t*)malloc(path_size * sizeof(char_t));
    if (path == NULL) {
        goto cleanup;
    }
    if (get_hostfxr_path(path, &path_size, &parameters) == 0) {
#if defined(_WIN32)
        library = (void*)LoadLibraryW((const wchar_t*)path);
#else
        library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }
cleanup:
    free(path);
#if defined(_WIN32)
    free(dotnet_root);
#endif
    return library;
}
