#pragma once

#include <SDL3/SDL.h>

inline bool oct_env_flag_enabled(const char* name)
{
    const char* value = SDL_getenv(name);
    return value && value[0] != '\0' && SDL_strcmp(value, "0") != 0 && SDL_strcasecmp(value, "false") != 0 &&
        SDL_strcasecmp(value, "off") != 0;
}
