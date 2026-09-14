#include "SlangRhiSwapchain.h"

#include <cstdio>
#include <cstring>

int main() {
  const auto result =
      octaryn::client::rendering::probe_slang_rhi_swapchain();

  if (!result.swapchain_created || !result.image_acquired ||
      !result.image_transitioned || !result.command_submitted ||
      !result.presented || !result.queue_idle ||
      std::strcmp(result.status, "swapchain_present_validated") != 0) {
    std::fprintf(stderr,
                 "client render backend swapchain probe failed: status=%s "
                 "driver=%s sdl=%d window=%d native_handle=%d device=%d queue=%d "
                 "swapchain=%d acquired=%d transitioned=%d submitted=%d "
                 "presented=%d idle=%d\n",
                 result.status, result.video_driver,
                 result.sdl_video_initialized ? 1 : 0,
                 result.window_created ? 1 : 0,
                 result.native_handle_available ? 1 : 0,
                 result.device_created ? 1 : 0,
                 result.queue_created ? 1 : 0,
                 result.swapchain_created ? 1 : 0,
                 result.image_acquired ? 1 : 0,
                 result.image_transitioned ? 1 : 0,
                 result.command_submitted ? 1 : 0,
                 result.presented ? 1 : 0, result.queue_idle ? 1 : 0);
    return 1;
  }

  std::printf("client_render_backend_swapchain_probe=passed driver=%s "
              "status=%s swapchain=created acquired=1 transitioned=1 "
              "submitted=1 presented=1 idle=1\n",
              result.video_driver, result.status);
  return 0;
}
