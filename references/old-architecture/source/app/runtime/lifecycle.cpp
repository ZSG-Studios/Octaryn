#include "app/runtime/lifecycle.h"

#include "app/runtime/events.h"
#include "app/runtime/iterate.h"
#include "app/runtime/shutdown.h"
#include "app/runtime/startup/startup.h"

SDL_AppResult app_runtime_init(void** appstate, int argc, char** argv)
{
    return app_runtime_startup(appstate, argc, argv);
}

void app_runtime_quit(void* appstate, SDL_AppResult result)
{
    app_runtime_shutdown(appstate, result);
}

SDL_AppResult app_runtime_frame(void* appstate)
{
    return app_runtime_iterate(appstate);
}

SDL_AppResult app_runtime_handle_event(void* appstate, SDL_Event* event)
{
    return app_runtime_event(appstate, event);
}
