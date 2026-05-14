#include "core/persistence/internal.h"

namespace persistence_json {

auto to_settings_file(const app_settings_blob& blob) -> settings_file
{
    settings_file file{};
    file.version = blob.version;
    file.fog_enabled = blob.fog_enabled != 0;
    file.fullscreen = blob.fullscreen != 0;
    file.display_name = blob.display_name;
    file.display_index = blob.display_index;
    file.display_mode_width = blob.display_mode_width;
    file.display_mode_height = blob.display_mode_height;
    file.display_mode_refresh_rate = blob.display_mode_refresh_rate;
    file.clouds_enabled = blob.clouds_enabled != 0;
    file.sky_gradient_enabled = blob.sky_gradient_enabled != 0;
    file.window_width = blob.window_width;
    file.window_height = blob.window_height;
    file.render_distance = blob.render_distance;
    file.worldgen_threads = blob.worldgen_threads;
    file.stars_enabled = blob.stars_enabled != 0;
    file.sun_enabled = blob.sun_enabled != 0;
    file.moon_enabled = blob.moon_enabled != 0;
    file.pom_enabled = blob.pom_enabled != 0;
    file.pbr_enabled = blob.pbr_enabled != 0;
    return file;
}

auto to_settings_blob(const settings_file& file) -> app_settings_blob
{
    app_settings_blob blob{};
    blob.version = file.version;
    blob.fog_enabled = file.fog_enabled;
    blob.fullscreen = file.fullscreen;
    SDL_snprintf(blob.display_name, sizeof(blob.display_name), "%s", file.display_name.c_str());
    blob.display_index = file.display_index;
    blob.display_mode_width = file.display_mode_width;
    blob.display_mode_height = file.display_mode_height;
    blob.display_mode_refresh_rate = file.display_mode_refresh_rate;
    blob.clouds_enabled = file.clouds_enabled;
    blob.sky_gradient_enabled = file.sky_gradient_enabled;
    blob.window_width = file.window_width;
    blob.window_height = file.window_height;
    blob.render_distance = file.render_distance;
    blob.worldgen_threads = file.worldgen_threads;
    blob.stars_enabled = file.stars_enabled;
    blob.sun_enabled = file.sun_enabled;
    blob.moon_enabled = file.moon_enabled;
    blob.pom_enabled = file.pom_enabled;
    blob.pbr_enabled = file.pbr_enabled;
    return blob;
}

auto to_lighting_tuning_file(const lighting_tuning_blob& blob) -> lighting_tuning_file
{
    lighting_tuning_file file{};
    file.version = blob.version;
    file.ambient_strength = blob.ambient_strength;
    file.sun_strength = blob.sun_strength;
    file.sun_fallback_strength = blob.sun_fallback_strength;
    file.fog_distance = blob.fog_distance;
    file.skylight_floor = blob.skylight_floor;
    return file;
}

auto to_lighting_tuning_blob(const lighting_tuning_file& file) -> lighting_tuning_blob
{
    lighting_tuning_blob blob{};
    blob.version = file.version;
    blob.ambient_strength = file.ambient_strength;
    blob.sun_strength = file.sun_strength;
    blob.sun_fallback_strength = file.sun_fallback_strength;
    blob.fog_distance = file.fog_distance;
    blob.skylight_floor = file.skylight_floor;
    return blob;
}

auto to_player_file(const player_blob& blob) -> player_file
{
    player_file file{};
    file.x = blob.x;
    file.y = blob.y;
    file.z = blob.z;
    file.pitch = blob.pitch;
    file.yaw = blob.yaw;
    file.block = blob.block;
    return file;
}

auto to_player_blob(const player_file& file) -> player_blob
{
    player_blob blob{};
    blob.x = file.x;
    blob.y = file.y;
    blob.z = file.z;
    blob.pitch = file.pitch;
    blob.yaw = file.yaw;
    blob.block = static_cast<block_t>(file.block);
    return blob;
}

auto to_world_time_file(const world_time_blob& blob) -> world_time_file
{
    world_time_file file{};
    file.version = blob.version;
    file.day_index = blob.day_index;
    file.seconds_of_day = blob.seconds_of_day;
    return file;
}

auto to_world_time_blob(const world_time_file& file) -> world_time_blob
{
    world_time_blob blob{};
    blob.version = file.version;
    blob.day_index = file.day_index;
    blob.seconds_of_day = file.seconds_of_day;
    return blob;
}

} // namespace persistence_json
