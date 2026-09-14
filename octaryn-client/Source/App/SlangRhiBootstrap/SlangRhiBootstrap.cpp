#include "FrameLoop.h"
#include "RenderBackend.h"
#include "SlangRhiSwapchain.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void write_line(FILE *log, const char *line) {
  std::puts(line);
  if (log != nullptr) {
    std::fprintf(log, "%s\n", line);
    std::fflush(log);
  }
}
} // namespace

int run_render_diagnostic() {
  const char *log_path = std::getenv("OCTARYN_CLIENT_APP_LOG_PATH");
  FILE *log = log_path != nullptr && log_path[0] != '\0'
                  ? std::fopen(log_path, "w")
                  : nullptr;
  const auto status =
      octaryn::client::rendering::active_render_backend_status();

  write_line(log, "crash_marker=/tmp/octaryn-crash-slang-rhi-bootstrap.marker");
  write_line(log, "renderer_cutover_stage=slang_rhi_bootstrap");
  char device_line[96]{};
  std::snprintf(device_line, sizeof(device_line),
                "slang_rhi_device=%s runtime_available=%d",
                status.slang_device_status,
                status.slang_runtime_available ? 1 : 0);
  write_line(log, device_line);
  char frame_line[160]{};
  std::snprintf(frame_line, sizeof(frame_line),
                "slang_rhi_frame_resources=%s validated=%d",
                status.slang_frame_resource_status,
                status.slang_frame_resources_validated ? 1 : 0);
  write_line(log, frame_line);
  char lifecycle_line[192]{};
  std::snprintf(lifecycle_line, sizeof(lifecycle_line),
                "slang_rhi_frame_lifecycle=%s begun=%d encoded=%d "
                "ended=%d submitted=%d",
                status.slang_frame_lifecycle_validated ? "validated" : "failed",
                status.slang_frame_begun ? 1 : 0,
                status.slang_frame_encoded ? 1 : 0,
                status.slang_frame_ended ? 1 : 0,
                status.slang_frame_submitted ? 1 : 0);
  write_line(log, lifecycle_line);
  const bool frame_loop_ok = octaryn::client::app::run_frame_loop(log);
  const auto swapchain =
      octaryn::client::rendering::probe_slang_rhi_swapchain();
  char swapchain_line[224]{};
  std::snprintf(swapchain_line, sizeof(swapchain_line),
                "slang_rhi_swapchain=%s driver=%s created=%d acquired=%d "
                "transitioned=%d submitted=%d presented=%d idle=%d",
                swapchain.status, swapchain.video_driver,
                swapchain.swapchain_created ? 1 : 0,
                swapchain.image_acquired ? 1 : 0,
                swapchain.image_transitioned ? 1 : 0,
                swapchain.command_submitted ? 1 : 0,
                swapchain.presented ? 1 : 0,
                swapchain.queue_idle ? 1 : 0);
  write_line(log, swapchain_line);
  char backend_line[192]{};
  std::snprintf(backend_line, sizeof(backend_line),
                "client_render_backend active=1 backend=%s "
                "shader_language=slang legacy_glsl=%d legacy_sdl_renderer=%d "
                "device_created=%d",
                octaryn::client::rendering::render_backend_name(status.kind),
                status.legacy_glsl_allowed ? 1 : 0,
                status.legacy_sdl_renderer_allowed ? 1 : 0,
                status.slang_device_created ? 1 : 0);
  write_line(log, backend_line);
  write_line(log, "client_voxel_renderer_rebuild active=0 raster_only=1 "
                  "packed_quads=1 gpu_driven=planned");
  write_line(log, "shutdown=0");

  if (log != nullptr) {
    std::fclose(log);
  }
  const bool swapchain_ok = swapchain.presented && swapchain.image_transitioned &&
                            swapchain.command_submitted && swapchain.queue_idle &&
                            std::strcmp(swapchain.status,
                                        "swapchain_present_validated") == 0;
  return frame_loop_ok && swapchain_ok ? 0 : 1;
}
