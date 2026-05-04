#include "HostEnvironment.h"

#include <csignal>

namespace {

constexpr const char* DefaultAppName = "Octaryn";

auto app_name_or_default(const char* app_name) -> const char*
{
    return app_name != nullptr && app_name[0] != '\0' ? app_name : DefaultAppName;
}

} // namespace

int host_environment_configure(const char* app_name)
{
    int configured = 1;

#if !defined(_WIN32)
    if (SDL_getenv("SDL_VIDEODRIVER") == nullptr &&
        SDL_setenv_unsafe("SDL_VIDEODRIVER", "wayland", 0) != 0)
    {
        configured = 0;
    }
#endif

#if defined(SIGHUP)
    std::signal(SIGHUP, SIG_IGN);
#endif

    if (!SDL_SetAppMetadata(app_name_or_default(app_name), nullptr, nullptr))
    {
        configured = 0;
    }

    if (!SDL_SetHint(SDL_HINT_ASSERT, "abort"))
    {
        configured = 0;
    }

    if (!SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1"))
    {
        configured = 0;
    }

    return configured;
}
