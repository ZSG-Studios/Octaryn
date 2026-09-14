#pragma once
#include <cstdint>
#include "RuntimeControls.h"
#include "FrameProfile.h"
namespace octaryn::client::rendering {
struct UiDrawData {
  std::uint32_t Index{};
  std::uint32_t DebugEnabled{};
  std::uint32_t FPSTenths{};
  std::uint32_t FrameTimeHundredths{};
  std::uint32_t ProfileFrameTimeHundredths{};
  std::uint32_t FPSAverageTenths{};
  std::uint32_t FPSLow1Tenths{};
  std::uint32_t FPSLow01Tenths{};
  std::uint32_t FPSLowX5Tenths{};
  std::uint32_t FPSLowX10Tenths{};
  std::uint32_t FPSWorstTenths{};
  std::uint32_t WarmupComplete{};
  std::uint32_t SampleCount{};
  std::uint32_t MSLow1Hundredths{};
  std::uint32_t MSLow01Hundredths{};
  std::uint32_t MSLowX5Hundredths{};
  std::uint32_t MSLowX10Hundredths{};
  std::uint32_t MSWorstHundredths{};
  std::uint32_t WarmupElapsedHundredths{};
  std::uint32_t WarmupTotalHundredths{};
  std::uint32_t SimTimeHundredths{};
  std::uint32_t MiscTimeHundredths{};
  std::uint32_t WorldTimeHundredths{};
  std::uint32_t RenderTimeHundredths{};
  std::uint32_t RenderSetupHundredths{};
  std::uint32_t RenderOtherTimeHundredths{};
  std::uint32_t GBufferTimeHundredths{};
  std::uint32_t GBufferSkyHundredths{};
  std::uint32_t GBufferOpaqueHundredths{};
  std::uint32_t GBufferSpriteHundredths{};
  std::uint32_t PostTimeHundredths{};
  std::uint32_t CompositeTimeHundredths{};
  std::uint32_t DepthTimeHundredths{};
  std::uint32_t ForwardTimeHundredths{};
  std::uint32_t UiTimeHundredths{};
  std::uint32_t ImGuiTimeHundredths{};
  std::uint32_t SwapchainBlitHundredths{};
  std::uint32_t RenderSubmitHundredths{};
  std::uint32_t UntrackedTimeHundredths{};
  std::uint32_t CpuRamHundredthsGiB{UINT32_MAX};
  std::uint32_t GpuVramHundredthsGiB{UINT32_MAX};
  std::uint32_t CpuLoadHundredths{UINT32_MAX};
  std::uint32_t GpuLoadHundredths{UINT32_MAX};
  std::uint32_t MenuEnabled{};
  std::uint32_t MenuRow{};
  std::uint32_t MenuDisplay{};
  std::uint32_t MenuModeWidth{};
  std::uint32_t MenuModeHeight{};
  std::uint32_t MenuFullscreen{};
  std::uint32_t MenuFog{};
  std::uint32_t MenuRenderDistance{};
  std::uint32_t MenuClouds{};
  std::uint32_t MenuSkyGradient{};
  std::uint32_t MenuStars{};
  std::uint32_t MenuSun{};
  std::uint32_t MenuMoon{};
  std::uint32_t MenuPOM{};
  std::uint32_t MenuPBR{};
};
UiDrawData make_ui_draw_data(const runtime_controls& controls);
void populate_ui_profile(UiDrawData& data, const frame_profile_snapshot& profile);
}
