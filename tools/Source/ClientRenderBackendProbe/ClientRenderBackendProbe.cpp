#include "RenderBackend.h"

#include <cstdio>

int main() {
  const auto status =
      octaryn::client::rendering::active_render_backend_status();

  if (!status.slang_runtime_available || !status.slang_device_created) {
    std::fprintf(stderr,
                 "client render backend probe failed: device status=%s "
                 "runtime_available=%d device_created=%d\n",
                 status.slang_device_status,
                 status.slang_runtime_available ? 1 : 0,
                 status.slang_device_created ? 1 : 0);
    return 1;
  }

  if (!status.slang_frame_resources_validated) {
    std::fprintf(stderr,
                 "client render backend probe failed: frame_resources=%s\n",
                 status.slang_frame_resource_status);
    return 1;
  }
  if (!status.slang_frame_lifecycle_validated) {
    std::fprintf(stderr,
                 "client render backend probe failed: frame_lifecycle "
                 "begun=%d encoded=%d ended=%d submitted=%d\n",
                 status.slang_frame_begun ? 1 : 0,
                 status.slang_frame_encoded ? 1 : 0,
                 status.slang_frame_ended ? 1 : 0,
                 status.slang_frame_submitted ? 1 : 0);
    return 1;
  }

  std::printf("client_render_backend_probe=passed backend=%s device=%s "
              "frame_resources=%s frame_lifecycle=validated\n",
              octaryn::client::rendering::render_backend_name(status.kind),
              status.slang_device_status,
              status.slang_frame_resource_status);
  return 0;
}
