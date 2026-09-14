#include "RuntimeSettings.h"

#include "AppSettings.h"
#include "DisplaySettings.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

struct client_app_settings_file {
    uint32_t version = APP_SETTINGS_VERSION;
    bool fogEnabled = true;
    bool fullscreen = false;
    std::string displayName;
    int32_t displayIndex = 0;
    int32_t displayModeWidth = 0;
    int32_t displayModeHeight = 0;
    float displayModeRefreshRate = 0.0f;
    bool cloudsEnabled = true;
    bool skyGradientEnabled = true;
    int32_t windowWidth = 0;
    int32_t windowHeight = 0;
    int32_t renderDistance = 16;
    bool starsEnabled = true;
    bool sunEnabled = true;
    bool moonEnabled = true;
    bool pomEnabled = true;
    bool pbrEnabled = true;
    bool rayTracingEnabled = true;
    unsigned upscalerMode = 0;
    uint8_t fsrSharpening = 1u;
    float fsrSharpness = 0.2f;
    float fsrRenderScale = 0.667f;
    uint8_t fsrDynamicResolution = 0u;
    float fsrMinScale = 0.5f;
    float fsrMaxScale = 1.0f;
    uint16_t fsrTargetFps = 60u;
    int32_t presentModeIndex = 0;
};

namespace {

constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts kJsonWriteOptions{.prettify = true};

auto settings_path() -> std::filesystem::path
{
    const char* override_path = SDL_getenv("OCTARYN_CLIENT_SETTINGS_PATH");
    if (override_path != nullptr && override_path[0] != '\0')
    {
        return std::filesystem::path(reinterpret_cast<const char8_t*>(override_path));
    }

    char* pref = SDL_GetPrefPath("ZSGStudios", "Octaryn");
    if (!pref) return std::filesystem::path("client-settings.json");
    const auto directory = std::filesystem::path(reinterpret_cast<const char8_t*>(pref));
    SDL_free(pref);
    return directory / "client-settings.json";
}

void copy_display_name(char output[APP_SETTINGS_DISPLAY_NAME_CAPACITY], const std::string& input)
{
    const auto length = std::min(input.size(), static_cast<size_t>(APP_SETTINGS_DISPLAY_NAME_CAPACITY - 1u));
    std::memcpy(output, input.data(), length);
    output[length] = '\0';
}

auto settings_file_from_settings(const app_settings& settings) -> client_app_settings_file
{
    client_app_settings_file file{};
    file.version = settings.version;
    file.fogEnabled = settings.fog_enabled != 0u;
    file.fullscreen = settings.fullscreen != 0u;
    file.displayName = settings.display_name;
    file.displayIndex = settings.display_index;
    file.displayModeWidth = settings.display_mode_width;
    file.displayModeHeight = settings.display_mode_height;
    file.displayModeRefreshRate = settings.display_mode_refresh_rate;
    file.cloudsEnabled = settings.clouds_enabled != 0u;
    file.skyGradientEnabled = settings.sky_gradient_enabled != 0u;
    file.windowWidth = settings.window_width;
    file.windowHeight = settings.window_height;
    file.renderDistance = settings.render_distance;
    file.starsEnabled = settings.stars_enabled != 0u;
    file.sunEnabled = settings.sun_enabled != 0u;
    file.moonEnabled = settings.moon_enabled != 0u;
    file.pomEnabled = settings.pom_enabled != 0u;
    file.pbrEnabled = settings.pbr_enabled != 0u;
    file.rayTracingEnabled = settings.ray_tracing_enabled != 0u;
    file.upscalerMode = settings.upscaler_mode;
    file.fsrSharpening = settings.fsr_sharpening;
    file.fsrSharpness = settings.fsr_sharpness;
    file.fsrRenderScale = settings.fsr_render_scale;
    file.fsrDynamicResolution = settings.fsr_dynamic_resolution;
    file.fsrMinScale = settings.fsr_min_scale;
    file.fsrMaxScale = settings.fsr_max_scale;
    file.fsrTargetFps = settings.fsr_target_fps;
    file.presentModeIndex = settings.present_mode_index;
    return file;
}

auto settings_from_file(const client_app_settings_file& file) -> app_settings
{
    app_settings settings{};
    app_settings_default(&settings);
    settings.version = file.version;
    settings.fog_enabled = file.fogEnabled ? 1u : 0u;
    settings.fullscreen = file.fullscreen ? 1u : 0u;
    copy_display_name(settings.display_name, file.displayName);
    settings.display_index = file.displayIndex;
    settings.display_mode_width = file.displayModeWidth;
    settings.display_mode_height = file.displayModeHeight;
    settings.display_mode_refresh_rate = file.displayModeRefreshRate;
    settings.clouds_enabled = file.cloudsEnabled ? 1u : 0u;
    settings.sky_gradient_enabled = file.skyGradientEnabled ? 1u : 0u;
    settings.window_width = file.windowWidth;
    settings.window_height = file.windowHeight;
    settings.render_distance = file.renderDistance;
    settings.stars_enabled = file.starsEnabled ? 1u : 0u;
    settings.sun_enabled = file.sunEnabled ? 1u : 0u;
    settings.moon_enabled = file.moonEnabled ? 1u : 0u;
    settings.pom_enabled = file.pomEnabled ? 1u : 0u;
    settings.pbr_enabled = file.pbrEnabled ? 1u : 0u;
    settings.ray_tracing_enabled = file.rayTracingEnabled ? 1u : 0u;
    settings.upscaler_mode = file.upscalerMode <= 6u ? static_cast<uint8_t>(file.upscalerMode) : 0u;
    settings.fsr_sharpening = file.fsrSharpening;
    settings.fsr_sharpness = file.fsrSharpness;
    settings.fsr_render_scale = file.fsrRenderScale;
    settings.fsr_dynamic_resolution = file.fsrDynamicResolution;
    settings.fsr_min_scale = file.fsrMinScale;
    settings.fsr_max_scale = file.fsrMaxScale;
    settings.fsr_target_fps = file.fsrTargetFps;
    settings.present_mode_index = file.presentModeIndex;
    return settings;
}

void apply_to_controls(const app_settings& settings, runtime_controls* controls)
{
    controls->fog_enabled = settings.fog_enabled;
    controls->clouds_enabled = settings.clouds_enabled;
    controls->sky_gradient_enabled = settings.sky_gradient_enabled;
    controls->stars_enabled = settings.stars_enabled;
    controls->sun_enabled = settings.sun_enabled;
    controls->moon_enabled = settings.moon_enabled;
    controls->pom_enabled = settings.pom_enabled;
    controls->pbr_enabled = settings.pbr_enabled;
    controls->ray_tracing_enabled = settings.ray_tracing_enabled;
    controls->upscaler_mode = settings.upscaler_mode;
    controls->fsr_sharpening = settings.fsr_sharpening;
    controls->fsr_sharpness = settings.fsr_sharpness;
    controls->fsr_render_scale = settings.fsr_render_scale;
    controls->fsr_dynamic_resolution = settings.fsr_dynamic_resolution;
    controls->fsr_min_scale = settings.fsr_min_scale;
    controls->fsr_max_scale = settings.fsr_max_scale;
    controls->fsr_target_fps = settings.fsr_target_fps;
    controls->render_distance = settings.render_distance;
    controls->present_mode_index = settings.present_mode_index;
}

void apply_to_window(const app_settings& settings, SDL_Window* window)
{
    if (window == nullptr)
    {
        return;
    }
    display_settings_restore_window(window, &settings);
}

auto settings_from_controls(SDL_Window* window, const runtime_controls* controls)
    -> app_settings
{
    app_settings settings{};
    app_settings_default(&settings);
    settings.fog_enabled = controls->fog_enabled;
    settings.clouds_enabled = controls->clouds_enabled;
    settings.sky_gradient_enabled = controls->sky_gradient_enabled;
    settings.stars_enabled = controls->stars_enabled;
    settings.sun_enabled = controls->sun_enabled;
    settings.moon_enabled = controls->moon_enabled;
    settings.pom_enabled = controls->pom_enabled;
    settings.pbr_enabled = controls->pbr_enabled;
    settings.ray_tracing_enabled = controls->ray_tracing_enabled;
    settings.upscaler_mode = controls->upscaler_mode;
    settings.fsr_sharpening = controls->fsr_sharpening;
    settings.fsr_sharpness = controls->fsr_sharpness;
    settings.fsr_render_scale = controls->fsr_render_scale;
    settings.fsr_dynamic_resolution = controls->fsr_dynamic_resolution;
    settings.fsr_min_scale = controls->fsr_min_scale;
    settings.fsr_max_scale = controls->fsr_max_scale;
    settings.fsr_target_fps = controls->fsr_target_fps;
    settings.render_distance = controls->render_distance;
    settings.present_mode_index = controls->present_mode_index;
    settings.display_index = controls->display_menu.display_index;

    if (window != nullptr)
    {
        settings.fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0u ? 1u : 0u;
        SDL_GetWindowSize(window, &settings.window_width, &settings.window_height);
    }
    display_settings_capture(&settings, window);
    return settings;
}

} // namespace

int runtime_settings_load(SDL_Window* window, runtime_controls* controls)
{
    if (controls == nullptr)
    {
        return 0;
    }

    const std::filesystem::path path = settings_path();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return 1;
    }

    const std::string payload((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    client_app_settings_file settings_file{};
    if (glz::read<kJsonReadOptions>(settings_file, payload))
    {
        return 0;
    }

    app_settings settings = settings_from_file(settings_file);
    if (app_settings_sanitize(&settings) == 0)
    {
        return 0;
    }

    apply_to_controls(settings, controls);
    apply_to_window(settings, window);
    return 1;
}

int runtime_settings_save(SDL_Window* window, const runtime_controls* controls)
{
    if (controls == nullptr)
    {
        return 0;
    }

    app_settings settings = settings_from_controls(window, controls);
    if (app_settings_sanitize(&settings) == 0)
    {
        return 0;
    }

    std::string output;
    if (glz::write<kJsonWriteOptions>(settings_file_from_settings(settings), output))
    {
        return 0;
    }

    const std::filesystem::path path = settings_path();
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return 0;
    }
    file.write(output.data(), static_cast<std::streamsize>(output.size()));
    return file.good() ? 1 : 0;
}
