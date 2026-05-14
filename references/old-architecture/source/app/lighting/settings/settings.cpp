#include "settings.h"

#include "internal.h"

main_lighting_tuning_t main_lighting_settings_default(void)
{
    main_lighting_tuning_t tuning = {};
    main_lighting_settings_init(&tuning);
    return tuning;
}