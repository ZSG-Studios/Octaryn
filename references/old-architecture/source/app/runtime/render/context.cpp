#include "app/runtime/render/context.h"

#include "app/runtime/internal.h"
#include "core/world_time/time.h"

namespace {

float smoothstep(float edge0, float edge1, float value)
{
    const float t = SDL_clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

bool app_resize_render_targets(int width, int height)
{
    if (!main_render_resources_resize(&resources, device, width, height))
    {
        return false;
    }
    camera_resize(&player.camera, static_cast<int>(resources.render_width), static_cast<int>(resources.render_height));
    return true;
}

void app_build_render_context(main_render_pass_context_t* render_context)
{
    const float visual_skylight = world_clock_snapshot.sky_visibility;
    const float gameplay_skylight = world_clock_snapshot.gameplay_sky_visibility;
    const float ambient_scale = 0.18f + SDL_powf(gameplay_skylight, 0.95f) * 0.82f;
    const float sun_scale = world_clock_snapshot.sunlight;
    const float visual_celestial_visibility = smoothstep(-0.28f, -0.08f, world_clock_snapshot.solar_elevation);
    float celestial_pitch = 0.0f;
    float celestial_yaw = 0.0f;
    world_time_get_celestial_angles(&world_clock_snapshot, &celestial_pitch, &celestial_yaw);
    const float celestial_cos_pitch = SDL_cosf(celestial_pitch);
    float visual_light_direction[3] = {
        celestial_cos_pitch * SDL_sinf(celestial_yaw),
        SDL_sinf(celestial_pitch),
        -celestial_cos_pitch * SDL_cosf(celestial_yaw),
    };
    const float skylight_floor = SDL_clamp(0.04f + lighting_tuning.skylight_floor * SDL_powf(visual_skylight, 1.02f),
                                           0.03f,
                                           lighting_tuning.skylight_floor);
    const float world_time_seconds = (float) SDL_fmod((double) ticks2 * 1.0e-9, 65536.0);
    int hidden_blocks[MAIN_RENDER_HIDDEN_BLOCK_CAPACITY][4] = {};
    const Uint32 hidden_block_count = static_cast<Uint32>(world_get_recent_hidden_blocks(hidden_blocks,
                                                                                          MAIN_RENDER_HIDDEN_BLOCK_CAPACITY));

    *render_context = {
        .resources = &resources,
        .pipelines = &pipelines,
        .player = &player,
        .fog_enabled = fog_enabled,
        .clouds_enabled = clouds_enabled,
        .sky_gradient_enabled = sky_gradient_enabled,
        .stars_enabled = stars_enabled,
        .sun_enabled = sun_enabled,
        .moon_enabled = moon_enabled,
        .pom_enabled = pom_enabled,
        .pbr_enabled = pbr_enabled,
        .fog_distance = lighting_tuning.fog_distance,
        .skylight_floor = skylight_floor,
        .ambient_strength = lighting_tuning.ambient_strength * (0.28f + ambient_scale * 0.72f),
        .sun_strength = lighting_tuning.sun_strength * sun_scale,
        .sun_fallback_strength = lighting_tuning.sun_fallback_strength * sun_scale,
        .visual_sky_visibility = visual_skylight,
        .twilight_strength = world_clock_snapshot.twilight,
        .celestial_visibility = visual_celestial_visibility,
        .world_time_seconds = world_time_seconds,
        .cloud_time_seconds = world_time_seconds,
        .cloud_max_distance = (float) (world_get_render_distance() * CHUNK_WIDTH * 2),
        .visual_light_direction = {visual_light_direction[0], visual_light_direction[1], visual_light_direction[2]},
        .hidden_block_count = hidden_block_count,
        .hidden_blocks = {},
    };

    for (Uint32 i = 0; i < hidden_block_count && i < MAIN_RENDER_HIDDEN_BLOCK_CAPACITY; ++i)
    {
        render_context->hidden_blocks[i][0] = hidden_blocks[i][0];
        render_context->hidden_blocks[i][1] = hidden_blocks[i][1];
        render_context->hidden_blocks[i][2] = hidden_blocks[i][2];
        render_context->hidden_blocks[i][3] = 0;
    }

}
