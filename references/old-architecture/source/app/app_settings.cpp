#include "app_settings.h"

#include "core/persistence/persistence.h"
#include "core/render_distance.h"
#include "world/runtime/world.h"

namespace {

auto app_settings_worker_limit() -> int
{
    const int cpu_count = SDL_GetNumLogicalCPUCores();
    const int detected = cpu_count > 0 ? cpu_count : 1;
    return SDL_clamp(detected, 1, OCTARYN_WORLD_JOBS_MAX_WORKERS);
}

} // namespace

namespace {

constexpr Uint32 kAppSettingsVersion = 7;

} // namespace

bool app_settings_load(app_settings_t* out_settings)
{
    if (out_settings == nullptr)
    {
        return false;
    }

    app_settings_t settings{};
    if (!persistence_get_settings(&settings, sizeof(settings)) || settings.version < 1 || settings.version > kAppSettingsVersion)
    {
        return false;
    }

    *out_settings = settings;
    out_settings->version = kAppSettingsVersion;
    out_settings->render_distance = octaryn_sanitize_render_distance(out_settings->render_distance);
    out_settings->worldgen_threads =
        out_settings->worldgen_threads > 0
            ? SDL_clamp(out_settings->worldgen_threads, 1, app_settings_worker_limit())
            : 0;
    return true;
}

void app_settings_save(const app_settings_t* settings)
{
    if (settings == nullptr)
    {
        return;
    }

    app_settings_t persisted = *settings;
    persisted.version = kAppSettingsVersion;
    persisted.render_distance = octaryn_sanitize_render_distance(persisted.render_distance);
    persisted.worldgen_threads = persisted.worldgen_threads > 0
                               ? SDL_clamp(persisted.worldgen_threads, 1, app_settings_worker_limit())
                               : 0;
    persistence_set_settings(&persisted, sizeof(persisted));
    persistence_commit();
}
