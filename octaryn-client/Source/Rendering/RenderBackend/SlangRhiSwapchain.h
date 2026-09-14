#pragma once

namespace octaryn::client::rendering {

struct SlangRhiSwapchainProbeResult {
  bool sdl_video_initialized;
  bool window_created;
  bool native_handle_available;
  bool device_created;
  bool queue_created;
  bool swapchain_created;
  bool image_acquired;
  bool image_transitioned;
  bool command_submitted;
  bool presented;
  bool queue_idle;
  const char *video_driver;
  const char *status;
};

SlangRhiSwapchainProbeResult probe_slang_rhi_swapchain();

} // namespace octaryn::client::rendering
