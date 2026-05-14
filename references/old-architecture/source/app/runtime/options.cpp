#include "app/runtime/options.h"

#include <SDL3/SDL.h>

namespace {

std::string app_runtime_default_save_path()
{
    char* pref_path = SDL_GetPrefPath("octaryn", "octaryn-engine");
    if (pref_path && pref_path[0] != '\0')
    {
        std::string path = pref_path;
        SDL_free(pref_path);
        return path;
    }

    if (pref_path)
    {
        SDL_free(pref_path);
    }
    return "savedata";
}

bool read_path_arg(const char* option, int argc, char** argv, int* index, std::string* output)
{
    if (*index + 1 >= argc)
    {
        SDL_Log("Missing value for %s", option);
        return false;
    }
    *output = argv[++(*index)];
    return true;
}

} // namespace

bool app_runtime_options_parse(app_runtime_options_t* options, int argc, char** argv)
{
    *options = {};
    options->save_path = app_runtime_default_save_path();

    for (int i = 1; i < argc; i++)
    {
        const char* arg = argv[i];
        if (SDL_strcmp(arg, "--save-path") == 0)
        {
            if (!read_path_arg(arg, argc, argv, &i, &options->save_path))
            {
                return false;
            }
            continue;
        }
        if (SDL_strcmp(arg, "--probe-visible-frame") == 0)
        {
            options->probes.require_visible_frame = true;
            continue;
        }
        if (SDL_strcmp(arg, "--dump-color") == 0)
        {
            options->probes.require_visible_frame = true;
            if (!read_path_arg(arg, argc, argv, &i, &options->probes.color_path))
            {
                return false;
            }
            continue;
        }
        if (SDL_strcmp(arg, "--dump-composite") == 0)
        {
            options->probes.require_visible_frame = true;
            if (!read_path_arg(arg, argc, argv, &i, &options->probes.composite_path))
            {
                return false;
            }
            continue;
        }
        SDL_Log("Unknown argument: %s", arg);
        return false;
    }
    return true;
}

bool app_runtime_probes_requested(const app_runtime_probe_options_t* probes)
{
    return probes &&
           ((probes->require_visible_frame && !probes->visible_frame_verified) ||
            (!probes->color_path.empty() && !probes->color_written) ||
            (!probes->composite_path.empty() && !probes->composite_written));
}
