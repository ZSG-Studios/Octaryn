#include "AppSettings.h"

#include "RenderDistance.h"

namespace {

auto normalize_flag(uint8_t value) -> uint8_t
{
    return value != 0u ? 1u : 0u;
}

auto sanitize_dimension(int32_t value) -> int32_t
{
    return value > 0 ? value : 0;
}

} // namespace

void app_settings_default(app_settings* settings)
{
    if (settings == nullptr)
    {
        return;
    }

    *settings = {};
    settings->version = APP_SETTINGS_VERSION;
    settings->fog_enabled = 1u;
    settings->fullscreen = 0u;
    settings->display_index = 0;
    settings->display_mode_width = 0;
    settings->display_mode_height = 0;
    settings->display_mode_refresh_rate = 0.0f;
    settings->clouds_enabled = 1u;
    settings->sky_gradient_enabled = 1u;
    settings->window_width = 0;
    settings->window_height = 0;
    settings->render_distance = 32;
    settings->stars_enabled = 1u;
    settings->sun_enabled = 1u;
    settings->moon_enabled = 1u;
    settings->pom_enabled = 1u;
    settings->pbr_enabled = 1u;
    settings->present_mode_index = 0;
}

int app_settings_is_supported_version(uint32_t version)
{
    return version >= 1u && version <= APP_SETTINGS_VERSION;
}

int app_settings_sanitize(app_settings* settings)
{
    if (settings == nullptr)
    {
        return 0;
    }

    if (!app_settings_is_supported_version(settings->version))
    {
        return 0;
    }

    settings->version = APP_SETTINGS_VERSION;
    settings->fog_enabled = normalize_flag(settings->fog_enabled);
    settings->fullscreen = normalize_flag(settings->fullscreen);
    settings->display_name[APP_SETTINGS_DISPLAY_NAME_CAPACITY - 1u] = '\0';
    if (settings->display_index < -1)
    {
        settings->display_index = -1;
    }
    settings->display_mode_width = sanitize_dimension(settings->display_mode_width);
    settings->display_mode_height = sanitize_dimension(settings->display_mode_height);
    if (settings->display_mode_refresh_rate < 0.0f)
    {
        settings->display_mode_refresh_rate = 0.0f;
    }
    settings->clouds_enabled = normalize_flag(settings->clouds_enabled);
    settings->sky_gradient_enabled = normalize_flag(settings->sky_gradient_enabled);
    settings->window_width = sanitize_dimension(settings->window_width);
    settings->window_height = sanitize_dimension(settings->window_height);
    settings->render_distance = render_distance_sanitize(settings->render_distance);
    settings->stars_enabled = normalize_flag(settings->stars_enabled);
    settings->sun_enabled = normalize_flag(settings->sun_enabled);
    settings->moon_enabled = normalize_flag(settings->moon_enabled);
    settings->pom_enabled = normalize_flag(settings->pom_enabled);
    settings->pbr_enabled = normalize_flag(settings->pbr_enabled);
    if (settings->present_mode_index < 0)
    {
        settings->present_mode_index = 0;
    }
    if (settings->present_mode_index > 2)
    {
        settings->present_mode_index = 2;
    }
    return 1;
}
