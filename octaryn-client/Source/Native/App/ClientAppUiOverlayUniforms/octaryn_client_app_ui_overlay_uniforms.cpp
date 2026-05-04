#include "octaryn_client_app_ui_overlay_uniforms.h"

#include "octaryn_client_render_distance.h"

#include <algorithm>
#include <climits>

namespace octaryn_client_app {

namespace {

using octaryn::client::rendering::client_block_atlas_top_layer_for_block;

int32_t clamp_int32(int32_t value, int32_t minimum, int32_t maximum) {
  return std::min(std::max(value, minimum), maximum);
}

} // namespace

ui_uniforms
build_ui_uniforms(const octaryn::client::rendering::ClientBlockAtlas &atlas,
                  const octaryn_client_runtime_controls &controls,
                  const octaryn_client_frame_profile_snapshot &profile,
                  uint16_t selected_place_block) {
  ui_uniforms uniforms{};
  const int32_t selected_layer =
      client_block_atlas_top_layer_for_block(atlas, selected_place_block);
  uniforms.index =
      selected_layer > 0 ? static_cast<uint32_t>(selected_layer) : 0u;
  uniforms.debug_enabled = controls.debug_overlay_enabled != 0u ? 1u : 0u;
  uniforms.fps_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.current.fps);
  uniforms.frame_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.metrics.current.ms);
  uniforms.profile_frame_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.total_ms);
  uniforms.fps_average_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.average.fps);
  uniforms.fps_low_1_tenths = octaryn_client_frame_profile_tenths_from_fps(
      profile.metrics.low_1pct.fps);
  uniforms.fps_low_0_1_tenths = octaryn_client_frame_profile_tenths_from_fps(
      profile.metrics.low_0_1pct.fps);
  uniforms.fps_low_x5_tenths = octaryn_client_frame_profile_tenths_from_fps(
      profile.metrics.confirmed_low_5.fps);
  uniforms.fps_low_x10_tenths = octaryn_client_frame_profile_tenths_from_fps(
      profile.metrics.confirmed_low_10.fps);
  uniforms.fps_worst_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.worst.fps);
  uniforms.warmup_complete = profile.metrics.warmup_complete;
  uniforms.sample_count =
      profile.metrics.sample_count > UINT32_MAX
          ? UINT32_MAX
          : static_cast<uint32_t>(profile.metrics.sample_count);
  uniforms.ms_low_1_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.metrics.low_1pct.ms);
  uniforms.ms_low_0_1_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.metrics.low_0_1pct.ms);
  uniforms.ms_low_x5_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.metrics.confirmed_low_5.ms);
  uniforms.ms_low_x10_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.metrics.confirmed_low_10.ms);
  uniforms.ms_worst_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.worst.ms);
  uniforms.warmup_elapsed_hundredths =
      octaryn_client_frame_profile_hundredths_from_seconds(
          profile.metrics.warmup_elapsed_seconds);
  uniforms.warmup_total_hundredths =
      octaryn_client_frame_profile_hundredths_from_seconds(
          profile.metrics.warmup_seconds);
  uniforms.sim_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.sim_ms);
  uniforms.misc_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.misc_ms);
  uniforms.world_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.world_ms);
  uniforms.render_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.render_ms);
  uniforms.render_setup_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.render_setup_ms);
  uniforms.render_other_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.render_other_ms);
  uniforms.gbuffer_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.gbuffer_ms);
  uniforms.gbuffer_sky_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.gbuffer_sky_ms);
  uniforms.gbuffer_opaque_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.gbuffer_opaque_ms);
  uniforms.gbuffer_sprite_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.gbuffer_sprite_ms);
  uniforms.post_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.post_ms);
  uniforms.composite_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.composite_ms);
  uniforms.depth_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.depth_ms);
  uniforms.forward_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.forward_ms);
  uniforms.ui_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.ui_ms);
  uniforms.imgui_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.imgui_ms);
  uniforms.swapchain_blit_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.swapchain_blit_ms);
  uniforms.render_submit_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.render_submit_ms);
  uniforms.untracked_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(
          profile.sample.untracked_ms);
  uniforms.menu_enabled = controls.display_menu.active != 0u ? 1u : 0u;
  uniforms.menu_row = static_cast<uint32_t>(clamp_int32(
      controls.display_menu.row, 0, OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT - 1));
  uniforms.menu_display =
      controls.display_menu.display_index >= 0
          ? static_cast<uint32_t>(controls.display_menu.display_index + 1)
          : 0u;
  if (controls.display_menu.mode_index >= 0 &&
      controls.display_menu.mode_index < controls.display_catalog.mode_count) {
    const octaryn_client_display_catalog_mode &mode =
        controls.display_catalog.modes[controls.display_menu.mode_index];
    uniforms.menu_mode_width = static_cast<uint32_t>(mode.pixel_width);
    uniforms.menu_mode_height = static_cast<uint32_t>(mode.pixel_height);
  }
  uniforms.menu_fullscreen = controls.display_menu.fullscreen;
  uniforms.menu_present_mode =
      controls.display_menu.present_mode_index >= 0
          ? static_cast<uint32_t>(controls.display_menu.present_mode_index)
          : 0u;
  uniforms.menu_fog = controls.display_menu.fog_enabled;
  const int *distance_options = octaryn_client_render_distance_options();
  const int distance_count = octaryn_client_render_distance_option_count();
  if (controls.display_menu.render_distance_index >= 0 &&
      controls.display_menu.render_distance_index < distance_count) {
    uniforms.menu_render_distance = static_cast<uint32_t>(
        distance_options[controls.display_menu.render_distance_index]);
  } else {
    uniforms.menu_render_distance =
        static_cast<uint32_t>(controls.render_distance);
  }
  uniforms.menu_clouds = controls.display_menu.clouds_enabled;
  uniforms.menu_sky_gradient = controls.display_menu.sky_gradient_enabled;
  uniforms.menu_stars = controls.display_menu.stars_enabled;
  uniforms.menu_sun = controls.display_menu.sun_enabled;
  uniforms.menu_moon = controls.display_menu.moon_enabled;
  uniforms.menu_pom = controls.display_menu.pom_enabled;
  uniforms.menu_pbr = controls.display_menu.pbr_enabled;
  return uniforms;
}

} // namespace octaryn_client_app
