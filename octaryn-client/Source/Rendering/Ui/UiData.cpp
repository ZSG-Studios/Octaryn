#include "UiData.h"
namespace octaryn::client::rendering {
UiDrawData make_ui_draw_data(const runtime_controls& controls) {
  UiDrawData data{};
  data.DebugEnabled=controls.debug_overlay_enabled;
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
