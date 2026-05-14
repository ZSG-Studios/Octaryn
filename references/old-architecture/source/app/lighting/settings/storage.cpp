#include "internal.h"

#include "core/persistence/persistence.h"
#include "core/persistence/scalar_types.h"

bool main_lighting_settings_load(main_lighting_tuning_t* tuning)
{
    persistence_json::lighting_tuning_blob blob = {};
    if (!persistence_get_lighting_tuning(&blob, sizeof(blob)))
    {
        main_lighting_settings_init(tuning);
        return false;
    }

    tuning->fog_enabled = 1;
    tuning->fog_distance = blob.fog_distance;
    tuning->skylight_floor = blob.skylight_floor;
    tuning->ambient_strength = blob.ambient_strength;
    tuning->sun_strength = blob.sun_strength;
    tuning->sun_fallback_strength = blob.sun_fallback_strength;
    main_lighting_settings_sanitize(tuning);
    return true;
}

void main_lighting_settings_save(const main_lighting_tuning_t* tuning)
{
    if (!tuning)
    {
        return;
    }

    main_lighting_tuning_t sanitized = *tuning;
    main_lighting_settings_sanitize(&sanitized);

    persistence_json::lighting_tuning_blob blob = {};
    blob.version = 1;
    blob.fog_distance = sanitized.fog_distance;
    blob.skylight_floor = sanitized.skylight_floor;
    blob.ambient_strength = sanitized.ambient_strength;
    blob.sun_strength = sanitized.sun_strength;
    blob.sun_fallback_strength = sanitized.sun_fallback_strength;
    persistence_set_lighting_tuning(&blob, sizeof(blob));
    persistence_commit();
}
