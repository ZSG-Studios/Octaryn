#pragma once

#include <SDL3/SDL.h>

typedef struct main_ui_draw_data
{
    Uint32 index;
    Uint32 show_debug;
    Uint32 fps_tenths;
    Uint32 frame_time_hundredths;
    Uint32 profile_frame_time_hundredths;
    Uint32 fps_average_tenths;
    Uint32 fps_low_1_tenths;
    Uint32 fps_low_0_1_tenths;
    Uint32 fps_low_x5_tenths;
    Uint32 fps_low_x10_tenths;
    Uint32 fps_worst_tenths;
    Uint32 warmup_complete;
    Uint32 sample_count;
    Uint32 ms_low_1_hundredths;
    Uint32 ms_low_0_1_hundredths;
    Uint32 ms_low_x5_hundredths;
    Uint32 ms_low_x10_hundredths;
    Uint32 ms_worst_hundredths;
    Uint32 warmup_elapsed_hundredths;
    Uint32 warmup_total_hundredths;
    Uint32 sim_time_hundredths;
    Uint32 misc_time_hundredths;
    Uint32 world_time_hundredths;
    Uint32 render_time_hundredths;
    Uint32 render_setup_hundredths;
    Uint32 render_other_time_hundredths;
    Uint32 gbuffer_time_hundredths;
    Uint32 gbuffer_sky_hundredths;
    Uint32 gbuffer_opaque_hundredths;
    Uint32 gbuffer_sprite_hundredths;
    Uint32 post_time_hundredths;
    Uint32 composite_time_hundredths;
    Uint32 depth_time_hundredths;
    Uint32 forward_time_hundredths;
    Uint32 ui_time_hundredths;
    Uint32 imgui_time_hundredths;
    Uint32 swapchain_blit_hundredths;
    Uint32 render_submit_hundredths;
    Uint32 untracked_time_hundredths;
    Uint32 cpu_ram_hundredths_gib;
    Uint32 gpu_vram_hundredths_gib;
    Uint32 cpu_load_hundredths;
    Uint32 gpu_load_hundredths;
    Uint32 menu_enabled;
    Uint32 menu_row;
    Uint32 menu_display;
    Uint32 menu_mode_width;
    Uint32 menu_mode_height;
    Uint32 menu_fullscreen;
    Uint32 menu_fog;
    Uint32 menu_render_distance;
    Uint32 menu_clouds;
    Uint32 menu_sky_gradient;
    Uint32 menu_stars;
    Uint32 menu_sun;
    Uint32 menu_moon;
    Uint32 menu_pom;
    Uint32 menu_pbr;
}
main_ui_draw_data_t;

typedef struct main_ui_context
{
    SDL_GPUTexture* composite_texture;
    SDL_GPUTexture* atlas_texture;
    SDL_GPUSampler* nearest_sampler;
    SDL_GPUComputePipeline* pipeline;
    Sint32 viewport_size[2];
    main_ui_draw_data_t data;
}
main_ui_context_t;

void main_ui_render(SDL_GPUCommandBuffer* cbuf, const main_ui_context_t* context);
