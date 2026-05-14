#include "app/runtime/startup/startup.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/startup/env.h"
#include "app/runtime/startup/process.h"
#include "app/runtime/startup/session.h"
#include "core/crash_diagnostics.h"
#include "core/log.h"
#include "core/memory_mimalloc.h"
#include "core/profile.h"

SDL_AppResult app_runtime_startup(void** appstate, int argc, char** argv)
{
    (void) appstate;
    OCT_PROFILE_ZONE("SDL_AppInit");

    const Uint64 startup_profile_start = app_profile_now();
    oct_log_init("runtime");
    oct_memory_init();
    oct_crash_diagnostics_init("runtime");
    app_startup_configure_environment();
    if (!app_runtime_options_parse(&runtime_options, argc, argv))
    {
        return SDL_APP_FAILURE;
    }
    if (!app_startup_init_process())
    {
        return SDL_APP_FAILURE;
    }
    if (!app_startup_init_session())
    {
        return SDL_APP_FAILURE;
    }
    oct_log_infof("Startup timing | SDL_AppInit total took %.2f ms", app_profile_elapsed_ms(startup_profile_start));
    return SDL_APP_CONTINUE;
}
