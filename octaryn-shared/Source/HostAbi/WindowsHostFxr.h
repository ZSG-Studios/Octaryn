#pragma once

#include <windows.h>
#include <nethost.h>
#include <stdint.h>
#include <stdlib.h>

static void* octaryn_open_windows_hostfxr(void)
{
    size_t path_size = 0;
    const int result = get_hostfxr_path(NULL, &path_size, NULL);
    if ((uint32_t)result != UINT32_C(0x80008098) || path_size == 0 ||
        path_size > SIZE_MAX / sizeof(char_t)) {
        return NULL;
    }

    char_t* path = (char_t*)malloc(path_size * sizeof(char_t));
    if (path == NULL) {
        return NULL;
    }

    void* library = NULL;
    if (get_hostfxr_path(path, &path_size, NULL) == 0) {
        library = (void*)LoadLibraryW((const wchar_t*)path);
    }
    free(path);
    return library;
}
