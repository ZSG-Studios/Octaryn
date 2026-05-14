#include "app/runtime/startup/env.h"

#include <csignal>

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/launch.h"
#include "core/log.h"

void app_startup_configure_environment(void)
{
    app_configure_sdl_logging();
#if !defined(_WIN32)
    if (!SDL_getenv("SDL_VIDEODRIVER"))
    {
        SDL_setenv_unsafe("SDL_VIDEODRIVER", "wayland", 0);
        oct_log_infof("Defaulting SDL_VIDEODRIVER=wayland for native Wayland runtime");
    }
#endif
#if defined(SIGHUP)
    std::signal(SIGHUP, SIG_IGN);
    oct_log_infof("Ignoring SIGHUP so launcher/session hangups do not terminate the runtime");
#endif
    SDL_SetAppMetadata(APP_NAME, NULL, NULL);
    SDL_SetHint(SDL_HINT_ASSERT, "abort");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    app_configure_launch_mode();
}
