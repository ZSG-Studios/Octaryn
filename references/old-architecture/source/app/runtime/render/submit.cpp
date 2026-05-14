#include "app/runtime/render/submit.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "app/runtime/render/present.h"
#include "app/runtime/render/probe.h"
#include "app/runtime/telemetry.h"
#include "core/profile.h"

namespace {

void render_ui(SDL_GPUCommandBuffer* cbuf)
{
    const main_frame_profile_snapshot_t profile_snapshot = main_frame_profile_snapshot(&frame_profile);
    const main_runtime_telemetry_snapshot_t telemetry_snapshot =
        show_debug_overlay ? app_runtime_telemetry_sample() : main_runtime_telemetry_snapshot_t{};
    main_ui_context_t ui = {
        .composite_texture = resources.composite_texture,
        .atlas_texture = resources.atlas_texture,
        .nearest_sampler = resources.nearest_sampler,
        .pipeline = pipelines.ui,
        .viewport_size = {player.camera.viewport_size[0], player.camera.viewport_size[1]},
        .data = {
            .index = (Uint32) block_get_index(player.block, DIRECTION_NORTH),
            .show_debug = show_debug_overlay,
            .fps_tenths = profile_snapshot.current_fps_tenths != 0u ?
                profile_snapshot.current_fps_tenths :
                main_window_debug_last_submitted_fps_tenths(&main_window),
            .frame_time_hundredths = profile_snapshot.current_ms_hundredths != 0u ?
                profile_snapshot.current_ms_hundredths :
                main_window_debug_last_submitted_frame_time_hundredths(&main_window),
            .profile_frame_time_hundredths = profile_snapshot.average_ms_hundredths,
            .fps_average_tenths = profile_snapshot.average_fps_tenths,
            .fps_low_1_tenths = profile_snapshot.low_1_fps_tenths,
            .fps_low_0_1_tenths = profile_snapshot.low_0_1_fps_tenths,
            .fps_low_x5_tenths = profile_snapshot.low_x5_fps_tenths,
            .fps_low_x10_tenths = profile_snapshot.low_x10_fps_tenths,
            .fps_worst_tenths = profile_snapshot.worst_fps_tenths,
            .warmup_complete = profile_snapshot.warmup_complete,
            .sample_count = profile_snapshot.sample_count,
            .ms_low_1_hundredths = profile_snapshot.low_1_ms_hundredths,
            .ms_low_0_1_hundredths = profile_snapshot.low_0_1_ms_hundredths,
            .ms_low_x5_hundredths = profile_snapshot.low_x5_ms_hundredths,
            .ms_low_x10_hundredths = profile_snapshot.low_x10_ms_hundredths,
            .ms_worst_hundredths = profile_snapshot.worst_ms_hundredths,
            .warmup_elapsed_hundredths = profile_snapshot.warmup_elapsed_hundredths,
            .warmup_total_hundredths = profile_snapshot.warmup_total_hundredths,
            .sim_time_hundredths = profile_snapshot.sim_hundredths,
            .misc_time_hundredths = profile_snapshot.misc_hundredths,
            .world_time_hundredths = profile_snapshot.world_hundredths,
            .render_time_hundredths = profile_snapshot.render_hundredths,
            .render_setup_hundredths = profile_snapshot.render_setup_hundredths,
            .render_other_time_hundredths = profile_snapshot.render_other_hundredths,
            .gbuffer_time_hundredths = profile_snapshot.gbuffer_hundredths,
            .gbuffer_sky_hundredths = profile_snapshot.gbuffer_sky_hundredths,
            .gbuffer_opaque_hundredths = profile_snapshot.gbuffer_opaque_hundredths,
            .gbuffer_sprite_hundredths = profile_snapshot.gbuffer_sprite_hundredths,
            .post_time_hundredths = profile_snapshot.post_hundredths,
            .composite_time_hundredths = profile_snapshot.composite_hundredths,
            .depth_time_hundredths = profile_snapshot.depth_hundredths,
            .forward_time_hundredths = profile_snapshot.forward_hundredths,
            .ui_time_hundredths = profile_snapshot.ui_hundredths,
            .imgui_time_hundredths = profile_snapshot.imgui_hundredths,
            .swapchain_blit_hundredths = profile_snapshot.swapchain_blit_hundredths,
            .render_submit_hundredths = profile_snapshot.render_submit_hundredths,
            .untracked_time_hundredths = profile_snapshot.untracked_hundredths,
            .cpu_ram_hundredths_gib = telemetry_snapshot.cpu_ram_hundredths_gib,
            .gpu_vram_hundredths_gib = telemetry_snapshot.gpu_vram_hundredths_gib,
            .cpu_load_hundredths = telemetry_snapshot.cpu_load_hundredths,
            .gpu_load_hundredths = telemetry_snapshot.gpu_load_hundredths,
            .menu_enabled = display_menu.active,
            .menu_row = (Uint32) display_menu.row,
            .menu_display = (Uint32) (display_menu.display_index + 1),
            .menu_mode_width = 0,
            .menu_mode_height = 0,
            .menu_fullscreen = display_menu.fullscreen,
            .menu_fog = display_menu.fog_enabled,
            .menu_render_distance = (Uint32) DISTANCE_OPTIONS[display_menu.render_distance_index],
            .menu_clouds = display_menu.clouds_enabled ? 1u : 0u,
            .menu_sky_gradient = display_menu.sky_gradient_enabled ? 1u : 0u,
            .menu_stars = display_menu.stars_enabled ? 1u : 0u,
            .menu_sun = display_menu.sun_enabled ? 1u : 0u,
            .menu_moon = display_menu.moon_enabled ? 1u : 0u,
            .menu_pom = display_menu.pom_enabled ? 1u : 0u,
            .menu_pbr = display_menu.pbr_enabled ? 1u : 0u,
        },
    };

    if (display_menu.mode_index >= 0 && display_menu.mode_index < display_menu.mode_count)
    {
        ui.data.menu_mode_width = (Uint32) main_display_menu_mode_pixel_width(&display_menu.modes[display_menu.mode_index]);
        ui.data.menu_mode_height = (Uint32) main_display_menu_mode_pixel_height(&display_menu.modes[display_menu.mode_index]);
    }
    main_ui_render(cbuf, &ui);
}

}  // namespace

void app_submit_render_frame(SDL_GPUCommandBuffer* cbuf,
                              SDL_GPUTexture* swapchain_texture,
                              main_frame_profile_sample_t* profile_sample)
{
    OCT_PROFILE_ZONE("render.submit_frame");
    Uint64 stage_start = app_profile_now();
    render_ui(cbuf);
    if (profile_sample)
    {
        profile_sample->ui_ms = app_profile_elapsed_ms(stage_start);
    }

    const bool imgui_overlay_enabled = lighting_tuning.visible;
    if (imgui_overlay_enabled)
    {
        stage_start = app_profile_now();
        main_imgui_lighting_render(cbuf, resources.imgui_texture);
        if (profile_sample)
        {
            profile_sample->imgui_ms = app_profile_elapsed_ms(stage_start);
        }
    }

    stage_start = app_profile_now();
    app_render_swapchain(cbuf, swapchain_texture, resources.imgui_texture, imgui_overlay_enabled);
    if (profile_sample)
    {
        profile_sample->swapchain_blit_ms = app_profile_elapsed_ms(stage_start);
    }

    stage_start = app_profile_now();
    if (app_runtime_probes_requested(&runtime_options.probes))
    {
        if (!app_render_probe_capture_requested(cbuf,
                                                player.camera.viewport_size[0],
                                                player.camera.viewport_size[1],
                                                &runtime_options.probes))
        {
            if (profile_sample)
            {
                profile_sample->render_submit_ms = app_profile_elapsed_ms(stage_start);
            }
            return;
        }
    }
    else
    {
        SDL_SubmitGPUCommandBuffer(cbuf);
    }
    if (profile_sample)
    {
        profile_sample->render_submit_ms = app_profile_elapsed_ms(stage_start);
        profile_sample->present_submitted = 1u;
    }
    const Uint64 submit_ticks = app_profile_now();
    main_frame_profile_note_present_submitted(profile_sample, submit_ticks);
    main_window_note_submitted_frame(&main_window, submit_ticks);
}
