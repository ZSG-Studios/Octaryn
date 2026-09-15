#include "RuntimeControls.h"

#include "Menu.h"
#include "RenderDistance.h"
#include <algorithm>

#if defined(RUNTIME_CONTROLS_USE_SDL3)

void runtime_controls_init(runtime_controls* controls)
{
    if (controls == nullptr)
    {
        return;
    }

    *controls = {};
    controls->debug_overlay_enabled = 0u;
    controls->fog_enabled = 1u;
    controls->clouds_enabled = 1u;
    controls->sky_gradient_enabled = 1u;
    controls->stars_enabled = 1u;
    controls->sun_enabled = 1u;
    controls->moon_enabled = 1u;
    controls->pom_enabled = 1u;
    controls->pbr_enabled = 1u;
    controls->ray_tracing_enabled = 1u;
    controls->upscaler_mode = 0u;
    controls->fsr_sharpening = 1u;
    controls->fsr_sharpness = 0.2f;
    controls->fsr_render_scale = 0.667f;
    controls->fsr_dynamic_resolution = 0u;
    controls->fsr_min_scale = 0.5f;
    controls->fsr_max_scale = 1.0f;
    controls->fsr_target_fps = 60u;
    controls->frame_cap_fps = 0u;
    controls->gi_voxel_radius = 6u;
    controls->gi_coarse_radius = 128u;
    controls->shadow_distance = 1024u;
    controls->reflection_distance = 1024u;
    controls->camera_mode = 0u;
    controls->present_mode_index = 0;
    controls->render_distance = 32;
    controls->maximum_render_distance = 32;
    controls->display_menu.screen = DISPLAY_MENU_SCREEN_SETTINGS;
}

void runtime_controls_set_max_render_distance(runtime_controls* controls, int32_t maximum)
{
    if (!controls) return;
    controls->maximum_render_distance = render_distance_sanitize(maximum);
    controls->render_distance = std::min(render_distance_sanitize(controls->render_distance),
                                         controls->maximum_render_distance);
    const int* options = render_distance_options();
    for (int i = 0; i < render_distance_option_count(); ++i)
        if (options[i] == controls->render_distance) controls->display_menu.render_distance_index = i;
}

uint8_t runtime_controls_ui_active(const runtime_controls* controls)
{
    return controls != nullptr && controls->display_menu.active != 0u ? 1u : 0u;
}

void runtime_controls_refresh_menu(
    runtime_controls* controls,
    SDL_Window* window,
    int32_t viewport_width,
    int32_t viewport_height)
{
    if (controls == nullptr)
    {
        return;
    }

    display_catalog_refresh(
        &controls->display_catalog,
        window,
        viewport_width,
        viewport_height);
    runtime_controls_copy_to_menu(controls, window);
}

void runtime_controls_sync_relative_mouse(
    runtime_controls* controls,
    SDL_Window* window)
{
    if (controls == nullptr || window == nullptr)
    {
        return;
    }

    if (runtime_controls_ui_active(controls) != 0u)
    {
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            controls->restore_relative_mouse_after_ui = 1u;
            SDL_SetWindowRelativeMouseMode(window, false);
        }
        return;
    }

    if (controls->restore_relative_mouse_after_ui != 0u)
    {
        SDL_SetWindowRelativeMouseMode(window, true);
        controls->restore_relative_mouse_after_ui = 0u;
    }
}

#endif
