#include "FrameRender.h"

#include "AtlasFallbackDraw.h"
#include "CompositePass.h"
#include "Environment.h"
#include "FrameTargets.h"
#include "Log.h"
#include "ShaderWorldPass.h"
#include "UiOverlayPass.h"
#include "FunctionProfile.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>

namespace octaryn_client_app {
namespace {

constexpr const char *kPixelValidationFlag =
    "OCTARYN_CLIENT_APP_VALIDATE_PIXELS";

bool g_gpu_path_logged;

} // namespace

bool present_frame(
    SDL_GPUDevice *device, SDL_Window *window,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const camera &camera,
    const client_block_raycast_hit &selection_hit, uint16_t selected_place_block,
    const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const server_world_time_state &world_time,
    const runtime_controls &controls,
    const frame_profile_snapshot &profile, uint64_t frame_index,
    frame_profile_sample *profile_sample) {
  function_profile_scope present_frame_profile_scope(
      "present_frame", frame_index, "sdl_gpu");
  const uint64_t render_start = SDL_GetTicksNS();
  const uint64_t command_acquire_start = render_start;
  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (profile_sample != nullptr) {
    profile_sample->command_acquire_ms =
        frame_profile_elapsed_ms_since(command_acquire_start);
  }
  if (command_buffer == nullptr) {
    log_line("gpu_command_buffer=failed");
    return false;
  }

  SDL_GPUTexture *swapchain_texture = nullptr;
  uint32_t target_width = 0u;
  uint32_t target_height = 0u;
  const uint64_t swapchain_acquire_start = SDL_GetTicksNS();
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                             &swapchain_texture, &target_width,
                                             &target_height) ||
      swapchain_texture == nullptr) {
    log_line("gpu_swapchain_acquire=failed");
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->swapchain_wait_ms =
        frame_profile_elapsed_ms_since(swapchain_acquire_start);
    profile_sample->swapchain_acquire_ms = profile_sample->swapchain_wait_ms;
    profile_sample->frame_acquire_ms =
        profile_sample->command_acquire_ms + profile_sample->swapchain_wait_ms;
  }

  if (!g_gpu_path_logged && g_log != nullptr) {
    std::fprintf(g_log, "gpu_render_path=SDL_GPU\n");
    std::fprintf(
        g_log, "gpu_swapchain_acquired width=%" PRIu32 " height=%" PRIu32 "\n",
        target_width, target_height);
    std::fflush(g_log);
    g_gpu_path_logged = true;
  }

  const bool validate_pixels = read_enabled_flag(kPixelValidationFlag);
  SDL_GPUTexture *frame_texture = nullptr;
  SDL_GPUTexture *color_texture = nullptr;
  SDL_GPUTexture *depth_texture = nullptr;
  SDL_GPUTexture *position_texture = nullptr;
  SDL_GPUTexture *voxel_texture = nullptr;
  SDL_GPUTexture *material_texture = nullptr;
  auto release_frame_targets = [&]() {
    if (color_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, color_texture);
      color_texture = nullptr;
    }
    if (material_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, material_texture);
      material_texture = nullptr;
    }
    if (voxel_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, voxel_texture);
      voxel_texture = nullptr;
    }
    if (position_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, position_texture);
      position_texture = nullptr;
    }
    if (depth_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, depth_texture);
      depth_texture = nullptr;
    }
  };
  constexpr SDL_GPUTextureFormat color_format =
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  const uint64_t render_setup_start = SDL_GetTicksNS();
  frame_texture = create_composite_frame_texture(device, color_format,
                                                 target_width, target_height);
  if (frame_texture == nullptr) {
    log_line("live_composite_texture=create_failed");
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  color_texture = create_frame_color_target(device, color_format, target_width,
                                            target_height);
  if (color_texture == nullptr) {
    log_line("gpu_color_texture=create_failed");
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  SDL_GPUTexture *render_texture = color_texture;

  SDL_GPUTextureCreateInfo depth_info{};
  depth_info.type = SDL_GPU_TEXTURETYPE_2D;
  depth_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_info.width = target_width;
  depth_info.height = target_height;
  depth_info.layer_count_or_depth = 1u;
  depth_info.num_levels = 1u;
  depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  depth_texture = SDL_CreateGPUTexture(device, &depth_info);
  if (depth_texture == nullptr) {
    log_line("gpu_depth_texture=create_failed");
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  position_texture = create_frame_color_target(
      device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, target_width,
      target_height);
  voxel_texture =
      create_frame_color_target(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                target_width, target_height);
  material_texture =
      create_frame_color_target(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                target_width, target_height);
  if (position_texture == nullptr || voxel_texture == nullptr ||
      material_texture == nullptr) {
    log_line("gpu_gbuffer_texture=create_failed");
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!clear_gpu_swapchain(command_buffer, render_texture)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->render_setup_ms =
        frame_profile_elapsed_ms_since(render_setup_start);
  }

  if (!draw_shader_world(command_buffer, render_texture, depth_texture,
                         position_texture, voxel_texture, material_texture,
                         atlas, pipelines, mesh_buffers, mesh_frame, camera,
                         selection_hit, world_time, controls, frame_index,
                         profile_sample)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  int drawn_tiles = 0;
  const bool world_mesh_active = pipelines.world != nullptr &&
                                 mesh_buffers.opaque_faces != nullptr &&
                                 !mesh_frame.opaque_faces.empty();
  if (!world_mesh_active) {
    if (!draw_atlas_fallback_blocks(command_buffer, render_texture,
                                    target_width, target_height, atlas, blocks,
                                    camera, drawn_tiles)) {
      release_frame_targets();
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      SDL_CancelGPUCommandBuffer(command_buffer);
      return false;
    }
  }

  if (!draw_material_atlas_probe(command_buffer, render_texture, target_width,
                                 target_height, atlas)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!run_composite_pass(command_buffer, color_texture, position_texture,
                          voxel_texture, material_texture, frame_texture,
                          pipelines, world_time, camera, controls, target_width,
                          target_height, frame_index, profile_sample)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  const uint64_t ui_start = SDL_GetTicksNS();
  if (!render_ui_overlay(command_buffer, frame_texture, atlas, pipelines,
                         controls, profile, selected_place_block, target_width,
                         target_height)) {
    release_frame_targets();
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->ui_ms =
        frame_profile_elapsed_ms_since(ui_start);
  }

  const uint64_t blit_start = SDL_GetTicksNS();
  if (!present_composite_to_swapchain(command_buffer, frame_texture,
                                      swapchain_texture, pipelines,
                                      frame_index)) {
    release_frame_targets();
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->swapchain_blit_ms =
        frame_profile_elapsed_ms_since(blit_start);
  }

  gpu_pixel_readback sky_pixel_readback{};
  if (validate_pixels &&
      !begin_sky_pixel_readback(device, command_buffer, frame_texture,
                                color_format, target_width, target_height,
                                sky_pixel_readback)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (validate_pixels) {
    const uint64_t submit_start = SDL_GetTicksNS();
    SDL_GPUFence *fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms =
          frame_profile_elapsed_ms_since(submit_start);
    }
    if (fence == nullptr) {
      if (sky_pixel_readback.transfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, sky_pixel_readback.transfer);
      }
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }

    SDL_GPUFence *fences[] = {fence};
    const bool waited = SDL_WaitForGPUFences(device, true, fences, 1u);
    SDL_ReleaseGPUFence(device, fence);
    if (!waited || !finish_sky_pixel_readback(device, sky_pixel_readback)) {
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      release_frame_targets();
      return false;
    }
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    release_frame_targets();
  } else {
    const uint64_t submit_start = SDL_GetTicksNS();
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
      SDL_ReleaseGPUTexture(device, frame_texture);
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms =
          frame_profile_elapsed_ms_since(submit_start);
    }
    SDL_ReleaseGPUTexture(device, frame_texture);
    release_frame_targets();
  }
  if (profile_sample != nullptr) {
    profile_sample->render_ms =
        frame_profile_elapsed_ms_since(render_start);
  }

  if (drawn_tiles != 0 && g_log != nullptr) {
    std::fprintf(g_log, "gpu_atlas_blits_drawn=%d\n", drawn_tiles);
    std::fflush(g_log);
  }
  if (g_log != nullptr && !blocks.empty()) {
    std::fprintf(g_log, "presented_block_count=%zu\n", blocks.size());
    std::fflush(g_log);
  }

  return true;
}

} // namespace octaryn_client_app
