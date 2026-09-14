#pragma once

namespace octaryn::client::rendering {

struct SlangRhiFrameResourceProbeResult {
  bool runtime_available;
  bool device_created;
  bool queue_created;
  bool transient_heap_created;
  bool command_buffer_created;
  bool buffer_created;
  bool offscreen_target_created;
  bool framebuffer_created;
  bool render_pass_created;
  bool frame_begun;
  bool upload_encoded;
  bool frame_encoded;
  bool frame_ended;
  bool submitted;
  bool fence_completed;
  bool readback_valid;
  const char *status;
};

SlangRhiFrameResourceProbeResult probe_slang_rhi_frame_resources();

} // namespace octaryn::client::rendering
