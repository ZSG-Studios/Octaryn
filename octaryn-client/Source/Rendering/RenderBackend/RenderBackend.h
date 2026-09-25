#pragma once

namespace octaryn::client::rendering {

enum class RenderBackendKind {
  SlangRhi,
};

struct RenderBackendStatus {
  RenderBackendKind kind;
  bool slang_shaders_required;
  bool legacy_glsl_allowed;
  bool legacy_sdl_renderer_allowed;
  bool slang_runtime_available;
  bool slang_device_created;
  bool slang_frame_resources_validated;
  bool slang_frame_begun;
  bool slang_frame_encoded;
  bool slang_frame_ended;
  bool slang_frame_submitted;
  bool slang_frame_lifecycle_validated;
  const char *slang_device_status;
  const char *slang_frame_resource_status;
};

RenderBackendStatus active_render_backend_status();
const char *render_backend_name(RenderBackendKind kind);

} // namespace octaryn::client::rendering
