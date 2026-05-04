#include "UiOverlayPass.h"

#include "Log.h"
#include "UiOverlayUniforms.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace octaryn_client_app {

namespace {

struct ui_viewport_uniforms {
  int32_t viewport[2]{};
  int32_t offset[2]{};
};

int32_t clamp_int32(int32_t value, int32_t minimum, int32_t maximum) {
  return std::min(std::max(value, minimum), maximum);
}

void dispatch_ui_rect(SDL_GPUCommandBuffer *command_buffer,
                      SDL_GPUComputePass *compute_pass, int32_t viewport_width,
                      int32_t viewport_height, int32_t x, int32_t y,
                      int32_t width, int32_t height) {
  const int32_t x0 = clamp_int32(x, 0, viewport_width);
  const int32_t y0 = clamp_int32(y, 0, viewport_height);
  const int32_t x1 = clamp_int32(x + width, 0, viewport_width);
  const int32_t y1 = clamp_int32(y + height, 0, viewport_height);
  if (x1 <= x0 || y1 <= y0) {
    return;
  }

  ui_viewport_uniforms viewport_uniform{};
  viewport_uniform.viewport[0] = viewport_width;
  viewport_uniform.viewport[1] = viewport_height;
  viewport_uniform.offset[0] = x0;
  viewport_uniform.offset[1] = y0;
  const uint32_t groups_x = static_cast<uint32_t>((x1 - x0 + 7) / 8);
  const uint32_t groups_y = static_cast<uint32_t>((y1 - y0 + 7) / 8);
  SDL_PushGPUComputeUniformData(command_buffer, 0u, &viewport_uniform,
                                sizeof(viewport_uniform));
  SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1u);
}

void dispatch_ui_regions(SDL_GPUCommandBuffer *command_buffer,
                         SDL_GPUComputePass *compute_pass,
                         const ui_uniforms &uniforms, int32_t viewport_width,
                         int32_t viewport_height) {
  if (viewport_width <= 0 || viewport_height <= 0) {
    return;
  }

  if (uniforms.menu_enabled != 0u) {
    dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                     viewport_height, 0, 0, viewport_width, viewport_height);
    return;
  }

  const float base_scale =
      std::max(static_cast<float>(viewport_width) / 1280.0f,
               static_cast<float>(viewport_height) / 720.0f);
  const float scale = base_scale * 2.0f;
  const int32_t block_start =
      static_cast<int32_t>(std::floor(10.0f * scale)) - 2;
  const int32_t block_end = static_cast<int32_t>(std::ceil(60.0f * scale)) + 2;
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, block_start, viewport_height - block_end,
                   block_end - block_start, block_end - block_start);

  const int32_t cross_width =
      static_cast<int32_t>(std::ceil(8.0f * base_scale)) + 2;
  const int32_t cross_thickness =
      static_cast<int32_t>(std::ceil(2.0f * base_scale)) + 2;
  const int32_t center_x = viewport_width / 2;
  const int32_t center_y = viewport_height / 2;
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, center_x - cross_width,
                   center_y - cross_thickness, cross_width * 2,
                   cross_thickness * 2);
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, center_x - cross_thickness,
                   center_y - cross_width, cross_thickness * 2,
                   cross_width * 2);

  if (uniforms.debug_enabled == 0u) {
    return;
  }

  const uint32_t font_scale = std::max(1u, static_cast<uint32_t>(scale + 0.5f));
  const int32_t padding = 4 * static_cast<int32_t>(font_scale);
  const int32_t margin = 6 * static_cast<int32_t>(font_scale);
  const int32_t content_width = 30 * 4 * static_cast<int32_t>(font_scale) -
                                static_cast<int32_t>(font_scale);
  const int32_t content_height = 16 * 6 * static_cast<int32_t>(font_scale) -
                                 static_cast<int32_t>(font_scale);
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, margin, margin, content_width + padding * 2,
                   content_height + padding * 2);
}

} // namespace

bool render_ui_overlay(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const client_shader_pipelines &pipelines,
    const runtime_controls &controls,
    const frame_profile_snapshot &profile,
    uint16_t selected_place_block, uint32_t target_width,
    uint32_t target_height) {
  if (pipelines.ui == nullptr || pipelines.atlas_sampler == nullptr ||
      target_texture == nullptr || atlas.color_texture == nullptr) {
    return true;
  }

  SDL_GPUStorageTextureReadWriteBinding write_textures[1]{};
  write_textures[0].texture = target_texture;
  SDL_GPUComputePass *compute_pass =
      SDL_BeginGPUComputePass(command_buffer, write_textures, 1u, nullptr, 0u);
  if (compute_pass == nullptr) {
    log_line("live_ui_compute_pass=failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding read_textures[1]{};
  read_textures[0].texture = atlas.color_texture;
  read_textures[0].sampler = pipelines.atlas_sampler;
  const ui_uniforms uniforms =
      build_ui_uniforms(atlas, controls, profile, selected_place_block);
  SDL_BindGPUComputePipeline(compute_pass, pipelines.ui);
  SDL_BindGPUComputeSamplers(compute_pass, 0u, read_textures, 1u);
  SDL_PushGPUComputeUniformData(command_buffer, 1u, &uniforms,
                                sizeof(uniforms));
  dispatch_ui_regions(command_buffer, compute_pass, uniforms,
                      static_cast<int32_t>(target_width),
                      static_cast<int32_t>(target_height));
  SDL_EndGPUComputePass(compute_pass);
  if (g_log != nullptr &&
      (uniforms.debug_enabled != 0u || uniforms.menu_enabled != 0u)) {
    std::fprintf(g_log,
                 "live_ui_overlay active=1 debug=%" PRIu32 " menu=%" PRIu32
                 " row=%" PRIu32 " render_distance=%" PRIu32 "\n",
                 uniforms.debug_enabled, uniforms.menu_enabled,
                 uniforms.menu_row, uniforms.menu_render_distance);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
