#include "app/overlay/display_menu.h"

#include "app/overlay/interaction.h"
#include "app/runtime/internal.h"

namespace {

void persist_display_menu_settings(void)
{
    main_runtime_settings_persist(&runtime_settings);
}

} // namespace

const main_display_menu_runtime_t* app_get_display_menu_runtime(void)
{
    static main_display_menu_runtime_t runtime;
    runtime.window = window;
    runtime.player = &player;
    runtime.fog_enabled = &fog_enabled;
    runtime.clouds_enabled = &clouds_enabled;
    runtime.sky_gradient_enabled = &sky_gradient_enabled;
    runtime.stars_enabled = &stars_enabled;
    runtime.sun_enabled = &sun_enabled;
    runtime.moon_enabled = &moon_enabled;
    runtime.pom_enabled = &pom_enabled;
    runtime.pbr_enabled = &pbr_enabled;
    runtime.distance_options = DISTANCE_OPTIONS;
    runtime.distance_option_count = DISTANCE_OPTION_COUNT;
    runtime.persist_settings = persist_display_menu_settings;
    return &runtime;
}

void app_open_display_menu(void)
{
    main_display_menu_open(&display_menu, app_get_display_menu_runtime());
    app_sync_relative_mouse_mode_for_ui();
}

void app_apply_display_menu_settings(void)
{
    main_display_menu_request_apply(&display_menu);
    main_display_menu_commit_if_requested(&display_menu, app_get_display_menu_runtime());
    app_sync_relative_mouse_mode_for_ui();
}

void app_close_display_menu(void)
{
    app_apply_display_menu_settings();
}
