#include "app/runtime/startup/process.h"

#include "app/runtime/startup/platform.h"

bool app_startup_init_process(void)
{
    return app_startup_init_platform();
}
