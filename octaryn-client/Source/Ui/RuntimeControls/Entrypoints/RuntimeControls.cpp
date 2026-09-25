#include "RuntimeControls.h"

#include <cstring>

void runtime_controls_init(runtime_controls* controls)
{
    if (controls == nullptr)
    {
        return;
    }
    // Mirrors app_settings_default so a missing settings file still boots a
    // fully specified renderer configuration.
    std::memset(controls, 0, sizeof(*controls));
    controls->fog_enabled = 1u;
    controls->clouds_enabled = 1u;
    controls->sky_gradient_enabled = 1u;
    controls->stars_enabled = 1u;
    controls->sun_enabled = 1u;
    controls->moon_enabled = 1u;
    controls->pom_enabled = 1u;
    controls->pbr_enabled = 1u;
    controls->ray_tracing_enabled = 1u;
    controls->raster_sun_shadows = 1u;
    controls->fsr_sharpening = 1u;
    controls->fsr_sharpness = 0.2f;
    controls->fsr_render_scale = 0.667f;
    controls->fsr_min_scale = 0.5f;
    controls->fsr_max_scale = 1.0f;
    controls->fsr_target_fps = 60u;
    controls->shadow_distance = 1024u;
    controls->reflection_distance = 1024u;
    controls->reflection_quality = 2u;
    controls->shadow_quality = 2u;
}
