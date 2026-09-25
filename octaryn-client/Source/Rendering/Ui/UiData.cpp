#include "UiData.h"
#include "RenderDistance.h"
#include <algorithm>
namespace octaryn::client::rendering {
UiDrawData make_ui_draw_data(const runtime_controls& controls) {
  UiDrawData data{};
  const auto& menu=controls.display_menu;
  data.DebugEnabled=controls.debug_overlay_enabled;
  data.MenuEnabled=menu.active;
  data.MenuRow=static_cast<uint32_t>(std::clamp(menu.row,0,DISPLAY_MENU_ROW_COUNT-1));
  data.MenuDisplay=static_cast<uint32_t>(std::max(0,menu.display_index)+1);
  if(menu.mode_index>=0 && menu.mode_index<controls.display_catalog.mode_count) {
    const auto& mode=controls.display_catalog.modes[menu.mode_index];
    data.MenuModeWidth=static_cast<uint32_t>(std::max(0,mode.pixel_width));
    data.MenuModeHeight=static_cast<uint32_t>(std::max(0,mode.pixel_height));
  }
  const int distance=std::clamp(menu.render_distance_index,0,render_distance_option_count()-1);
  data.MenuRenderDistance=static_cast<uint32_t>(std::max(0,render_distance_options()[distance]));
  data.MenuFullscreen=menu.fullscreen;data.MenuFog=menu.fog_enabled;
  data.MenuClouds=menu.clouds_enabled;data.MenuSkyGradient=menu.sky_gradient_enabled;
  data.MenuStars=menu.stars_enabled;data.MenuSun=menu.sun_enabled;data.MenuMoon=menu.moon_enabled;
  data.MenuPOM=menu.pom_enabled;data.MenuPBR=menu.pbr_enabled;
  return data;
}
void populate_ui_profile(UiDrawData& data, const frame_profile_snapshot& profile) {
  const auto& m=profile.metrics;
  const auto& sample=profile.sample;
  data.FPSTenths=frame_profile_tenths_from_fps(m.current.fps);
  data.FrameTimeHundredths=frame_profile_hundredths_from_ms(m.current.ms);
  data.ProfileFrameTimeHundredths=frame_profile_hundredths_from_ms(m.average.ms);
  data.FPSAverageTenths=frame_profile_tenths_from_fps(m.average.fps);
  data.FPSLow1Tenths=frame_profile_tenths_from_fps(m.low_1pct.fps);
  data.FPSLow01Tenths=frame_profile_tenths_from_fps(m.low_0_1pct.fps);
  data.FPSLowX5Tenths=frame_profile_tenths_from_fps(m.confirmed_low_5.fps);
  data.FPSLowX10Tenths=frame_profile_tenths_from_fps(m.confirmed_low_10.fps);
  data.FPSWorstTenths=frame_profile_tenths_from_fps(m.worst.fps);
  data.MSLow1Hundredths=frame_profile_hundredths_from_ms(m.low_1pct.ms);
  data.MSLow01Hundredths=frame_profile_hundredths_from_ms(m.low_0_1pct.ms);
  data.MSLowX5Hundredths=frame_profile_hundredths_from_ms(m.confirmed_low_5.ms);
  data.MSLowX10Hundredths=frame_profile_hundredths_from_ms(m.confirmed_low_10.ms);
  data.MSWorstHundredths=frame_profile_hundredths_from_ms(m.worst.ms);
  data.WarmupComplete=m.warmup_complete;
  data.WarmupElapsedHundredths=frame_profile_hundredths_from_seconds(m.warmup_elapsed_seconds);
  data.WarmupTotalHundredths=frame_profile_hundredths_from_seconds(m.warmup_seconds);
  data.SampleCount=static_cast<uint32_t>(std::min<uint64_t>(m.sample_count,UINT32_MAX));
  data.SimTimeHundredths=frame_profile_hundredths_from_ms(sample.sim_ms);
  data.MiscTimeHundredths=frame_profile_hundredths_from_ms(sample.misc_ms);
  data.WorldTimeHundredths=frame_profile_hundredths_from_ms(sample.world_ms);
  data.RenderTimeHundredths=frame_profile_hundredths_from_ms(sample.render_ms);
  data.RenderSetupHundredths=frame_profile_hundredths_from_ms(sample.render_setup_ms);
  data.RenderOtherTimeHundredths=frame_profile_hundredths_from_ms(sample.render_other_ms);
  data.GBufferTimeHundredths=frame_profile_hundredths_from_ms(sample.gbuffer_ms);
  data.GBufferSkyHundredths=frame_profile_hundredths_from_ms(sample.gbuffer_sky_ms);
  data.GBufferOpaqueHundredths=frame_profile_hundredths_from_ms(sample.gbuffer_opaque_ms);
  data.GBufferSpriteHundredths=frame_profile_hundredths_from_ms(sample.gbuffer_sprite_ms);
  data.PostTimeHundredths=frame_profile_hundredths_from_ms(sample.post_ms);
  data.CompositeTimeHundredths=frame_profile_hundredths_from_ms(sample.composite_ms);
  data.DepthTimeHundredths=frame_profile_hundredths_from_ms(sample.depth_ms);
  data.ForwardTimeHundredths=frame_profile_hundredths_from_ms(sample.forward_ms);
  data.UiTimeHundredths=frame_profile_hundredths_from_ms(sample.ui_ms);
  data.ImGuiTimeHundredths=frame_profile_hundredths_from_ms(sample.imgui_ms);
  data.SwapchainBlitHundredths=frame_profile_hundredths_from_ms(sample.swapchain_blit_ms);
  data.RenderSubmitHundredths=frame_profile_hundredths_from_ms(sample.render_submit_ms);
  data.UntrackedTimeHundredths=frame_profile_hundredths_from_ms(sample.untracked_ms);
}

}
