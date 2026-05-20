#include "FrameRender.h"

#include "AtlasFallbackDraw.h"
#include "CompositePass.h"
#include "Environment.h"
#include "FrameTargets.h"
#include "Log.h"
#include "PlayerModelPass.h"
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
constexpr const char *kTerrainVisualAuditFlag =
    "OCTARYN_CLIENT_TERRAIN_VISUAL_AUDIT";
constexpr uint32_t kTerrainVisualAuditMaxSamples = 96u;

bool g_gpu_path_logged;
bool g_pixel_validation_completed;
uint32_t g_terrain_visual_audit_samples;
uint64_t g_terrain_visual_audit_start_frame;

} // namespace

bool present_frame(
    SDL_GPUDevice *device, SDL_Window *window,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const camera &player_camera,
    const camera &camera,
    const client_block_raycast_hit &selection_hit, uint16_t selected_place_block,
    const client_input_debug_state &input,
    const client_shader_pipelines &pipelines,
    frame_render_targets &targets,
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
  const float command_acquire_ms =
      frame_profile_elapsed_ms_since(command_acquire_start);
  if (profile_sample != nullptr) {
    profile_sample->command_acquire_ms = command_acquire_ms;
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

  const bool validate_pixels =
      read_enabled_flag(kPixelValidationFlag) && !g_pixel_validation_completed;
  constexpr SDL_GPUTextureFormat color_format =
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  const uint64_t render_setup_start = SDL_GetTicksNS();
  if (!ensure_frame_render_targets(device, color_format, target_width,
                                   target_height, targets)) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  SDL_GPUTexture *frame_texture = targets.frame;
  SDL_GPUTexture *color_texture = targets.color;
  SDL_GPUTexture *depth_texture = targets.depth;
  SDL_GPUTexture *position_texture = targets.position;
  SDL_GPUTexture *voxel_texture = targets.voxel;
  SDL_GPUTexture *material_texture = targets.material;
  SDL_GPUTexture *render_texture = color_texture;
  auto release_frame_targets = [&]() {};

  if (!clear_gpu_swapchain(command_buffer, render_texture)) {
    release_frame_targets();
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->render_setup_ms =
        frame_profile_elapsed_ms_since(render_setup_start);
  }
  const float setup_clear_ms =
      frame_profile_elapsed_ms_since(render_setup_start);

  const uint64_t world_pass_start = SDL_GetTicksNS();
  const bool session_active = controls.session_active != 0u;
  if (session_active &&
      !draw_shader_world(command_buffer, render_texture, depth_texture,
                         position_texture, voxel_texture, material_texture,
                         atlas, pipelines, mesh_buffers, mesh_frame, camera,
                         selection_hit, world_time, controls, frame_index,
                         profile_sample)) {
    release_frame_targets();
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  const float world_pass_ms = frame_profile_elapsed_ms_since(world_pass_start);

  if (!render_player_model(command_buffer, render_texture, depth_texture,
                           position_texture, voxel_texture, material_texture,
                           pipelines, player_camera, camera, input, controls,
                           frame_index)) {
    release_frame_targets();
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  int drawn_tiles = 0;
  float fallback_ms = 0.0f;
  const bool world_mesh_active = session_active && pipelines.world != nullptr &&
                                 world_mesh_gpu_has_geometry(mesh_buffers);
  const bool edit_input_active =
      (input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u;
  if (world_mesh_active && edit_input_active &&
      g_terrain_visual_audit_start_frame == 0u) {
    g_terrain_visual_audit_start_frame = frame_index;
  }
  const bool terrain_visual_audit_started =
      g_terrain_visual_audit_start_frame != 0u &&
      frame_index >= g_terrain_visual_audit_start_frame;
  const bool terrain_visual_audit =
      world_mesh_active && mesh_buffers.chunks.size() >= 512u &&
      terrain_visual_audit_started &&
      read_enabled_flag(kTerrainVisualAuditFlag) &&
      frame_index % 30u == 0u &&
      g_terrain_visual_audit_samples < kTerrainVisualAuditMaxSamples;
  const bool frame_readback = validate_pixels || terrain_visual_audit;
  if (session_active && !world_mesh_active) {
    const uint64_t fallback_start = SDL_GetTicksNS();
    if (!draw_atlas_fallback_blocks(command_buffer, render_texture,
                                    target_width, target_height, atlas, blocks,
                                    camera, drawn_tiles)) {
      release_frame_targets();
      SDL_CancelGPUCommandBuffer(command_buffer);
      return false;
    }
    fallback_ms = frame_profile_elapsed_ms_since(fallback_start);
  }

  const uint64_t atlas_probe_start = SDL_GetTicksNS();
  if (!draw_material_atlas_probe(command_buffer, render_texture, target_width,
                                 target_height, atlas)) {
    release_frame_targets();
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  const float atlas_probe_ms =
      frame_profile_elapsed_ms_since(atlas_probe_start);

  const uint64_t composite_start = SDL_GetTicksNS();
  if (!run_composite_pass(command_buffer, color_texture, position_texture,
                          voxel_texture, material_texture, frame_texture,
                          pipelines, world_time, camera, controls, target_width,
                          target_height, frame_index, profile_sample)) {
    release_frame_targets();
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  const float composite_pass_ms =
      frame_profile_elapsed_ms_since(composite_start);

  const uint64_t ui_start = SDL_GetTicksNS();
  if (!render_ui_overlay(command_buffer, frame_texture, atlas, pipelines,
                         controls, profile, selected_place_block, target_width,
                         target_height)) {
    release_frame_targets();
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
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->swapchain_blit_ms =
        frame_profile_elapsed_ms_since(blit_start);
  }

  gpu_pixel_readback sky_pixel_readback{};
  float readback_begin_ms = 0.0f;
  if (frame_readback) {
    const uint64_t readback_begin_start = SDL_GetTicksNS();
    const bool readback_started =
        begin_sky_pixel_readback(device, command_buffer, frame_texture,
                                 color_format, target_width, target_height,
                                 sky_pixel_readback);
    readback_begin_ms = frame_profile_elapsed_ms_since(readback_begin_start);
    if (!readback_started) {
      release_frame_targets();
      SDL_CancelGPUCommandBuffer(command_buffer);
      return false;
    }
  }

  float submit_ms = 0.0f;
  float fence_wait_ms = 0.0f;
  float readback_finish_ms = 0.0f;
  if (frame_readback) {
    const uint64_t submit_start = SDL_GetTicksNS();
    SDL_GPUFence *fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    submit_ms = frame_profile_elapsed_ms_since(submit_start);
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms = submit_ms;
    }
    if (fence == nullptr) {
      if (sky_pixel_readback.transfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, sky_pixel_readback.transfer);
      }
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }

    SDL_GPUFence *fences[] = {fence};
    const uint64_t fence_wait_start = SDL_GetTicksNS();
    const bool waited = SDL_WaitForGPUFences(device, true, fences, 1u);
    fence_wait_ms = frame_profile_elapsed_ms_since(fence_wait_start);
    SDL_ReleaseGPUFence(device, fence);
    const uint64_t readback_finish_start = SDL_GetTicksNS();
    const bool readback_finished =
        terrain_visual_audit
            ? finish_terrain_visual_readback(device, sky_pixel_readback,
                                             frame_index)
            : finish_sky_pixel_readback(device, sky_pixel_readback);
    readback_finish_ms = frame_profile_elapsed_ms_since(readback_finish_start);
    if (!waited || !readback_finished) {
      release_frame_targets();
      return false;
    }
    if (terrain_visual_audit) {
      ++g_terrain_visual_audit_samples;
    } else {
      g_pixel_validation_completed = true;
    }
    release_frame_targets();
  } else {
    const uint64_t submit_start = SDL_GetTicksNS();
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }
    submit_ms = frame_profile_elapsed_ms_since(submit_start);
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms = submit_ms;
    }
    release_frame_targets();
  }
  const float render_ms = frame_profile_elapsed_ms_since(render_start);
  if (profile_sample != nullptr) {
    profile_sample->render_ms = render_ms;
  }

  if (g_log != nullptr && (frame_index <= 5u || frame_index % 60u == 0u ||
                           render_ms >= 4.0f)) {
    std::fprintf(
        g_log,
        "live_render_phase_profile frame=%" PRIu64
        " total_ms=%.3f command_acquire_ms=%.3f swapchain_wait_ms=%.3f"
        " setup_clear_ms=%.3f world_pass_ms=%.3f fallback_ms=%.3f"
        " atlas_probe_ms=%.3f composite_pass_ms=%.3f ui_ms=%.3f"
        " blit_ms=%.3f readback_begin_ms=%.3f submit_ms=%.3f"
        " fence_wait_ms=%.3f readback_finish_ms=%.3f validate_pixels=%d"
        " world_mesh_active=%d drawn_tiles=%d\n",
        frame_index, render_ms, command_acquire_ms,
        profile_sample != nullptr ? profile_sample->swapchain_wait_ms : 0.0f,
        setup_clear_ms, world_pass_ms, fallback_ms, atlas_probe_ms,
        composite_pass_ms,
        profile_sample != nullptr ? profile_sample->ui_ms : 0.0f,
        profile_sample != nullptr ? profile_sample->swapchain_blit_ms : 0.0f,
        readback_begin_ms, submit_ms, fence_wait_ms, readback_finish_ms,
        validate_pixels ? 1 : 0, world_mesh_active ? 1 : 0, drawn_tiles);
    std::fflush(g_log);
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
