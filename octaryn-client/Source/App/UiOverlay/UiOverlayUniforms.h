#pragma once

#include "BlockAtlas.h"
#include "FrameProfile.h"
#include "RuntimeControls.h"

#include <cstdint>

namespace octaryn_client_app {

struct ui_uniforms {
  uint32_t index = 0u;
  uint32_t debug_enabled = 0u;
  uint32_t fps_tenths = 0u;
  uint32_t frame_time_hundredths = 0u;
  uint32_t profile_frame_time_hundredths = 0u;
  uint32_t fps_average_tenths = 0u;
  uint32_t fps_low_1_tenths = 0u;
  uint32_t fps_low_0_1_tenths = 0u;
  uint32_t fps_low_x5_tenths = 0u;
  uint32_t fps_low_x10_tenths = 0u;
  uint32_t fps_worst_tenths = 0u;
  uint32_t warmup_complete = 1u;
  uint32_t sample_count = 0u;
  uint32_t ms_low_1_hundredths = 0u;
  uint32_t ms_low_0_1_hundredths = 0u;
  uint32_t ms_low_x5_hundredths = 0u;
  uint32_t ms_low_x10_hundredths = 0u;
  uint32_t ms_worst_hundredths = 0u;
  uint32_t warmup_elapsed_hundredths = 0u;
  uint32_t warmup_total_hundredths = 0u;
  uint32_t sim_time_hundredths = 0u;
  uint32_t misc_time_hundredths = 0u;
  uint32_t world_time_hundredths = 0u;
  uint32_t render_time_hundredths = 0u;
  uint32_t render_setup_hundredths = 0u;
  uint32_t render_other_time_hundredths = 0u;
  uint32_t gbuffer_time_hundredths = 0u;
  uint32_t gbuffer_sky_hundredths = 0u;
  uint32_t gbuffer_opaque_hundredths = 0u;
  uint32_t gbuffer_sprite_hundredths = 0u;
  uint32_t post_time_hundredths = 0u;
  uint32_t composite_time_hundredths = 0u;
  uint32_t depth_time_hundredths = 0u;
  uint32_t forward_time_hundredths = 0u;
  uint32_t ui_time_hundredths = 0u;
  uint32_t imgui_time_hundredths = 0u;
  uint32_t swapchain_blit_hundredths = 0u;
  uint32_t render_submit_hundredths = 0u;
  uint32_t untracked_time_hundredths = 0u;
  uint32_t cpu_ram_hundredths_gib = 0u;
  uint32_t gpu_vram_hundredths_gib = 0u;
  uint32_t cpu_load_hundredths = 0u;
  uint32_t gpu_load_hundredths = 0u;
  uint32_t menu_enabled = 0u;
  uint32_t menu_row = 0u;
  uint32_t menu_display = 0u;
  uint32_t menu_mode_width = 0u;
  uint32_t menu_mode_height = 0u;
  uint32_t menu_fullscreen = 0u;
  uint32_t menu_present_mode = 0u;
  uint32_t menu_fog = 0u;
  uint32_t menu_render_distance = 0u;
  uint32_t menu_clouds = 0u;
  uint32_t menu_sky_gradient = 0u;
  uint32_t menu_stars = 0u;
  uint32_t menu_sun = 0u;
  uint32_t menu_moon = 0u;
  uint32_t menu_pom = 0u;
  uint32_t menu_pbr = 0u;
};

ui_uniforms
build_ui_uniforms(const octaryn::client::rendering::BlockAtlas &atlas,
                  const runtime_controls &controls,
                  const frame_profile_snapshot &profile,
                  uint16_t selected_place_block);

} // namespace octaryn_client_app
