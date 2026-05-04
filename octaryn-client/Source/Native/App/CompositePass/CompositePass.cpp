#include "CompositePass.h"

#include "Log.h"
#include "SkyUniforms.h"
#include "octaryn_client_function_profile.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace octaryn_client_app {

namespace {

struct composite_uniforms {
  float sky_visibility_and_ambient_strength[4]{};
};

bool g_composite_path_logged;
bool g_present_path_logged;

float build_composite_ambient_strength(float visual_sky_visibility) {
  const float gameplay_skylight = clamp01(visual_sky_visibility);
  const float ambient_scale =
      0.18f + std::pow(gameplay_skylight, 0.95f) * 0.82f;
  return 0.82f * (0.28f + ambient_scale * 0.72f);
}

composite_uniforms
build_composite_uniforms(const server_world_time_state &world_time,
                         const octaryn_client_camera &camera,
                         const octaryn_client_runtime_controls &controls) {
  const sky_uniforms sky = build_sky_uniforms(world_time, camera, controls);
  const float visual_sky_visibility = sky.light_direction_and_sky_visibility[3];
  composite_uniforms uniforms{};
  uniforms.sky_visibility_and_ambient_strength[0] = visual_sky_visibility;
  uniforms.sky_visibility_and_ambient_strength[1] =
      build_composite_ambient_strength(visual_sky_visibility);
  uniforms.sky_visibility_and_ambient_strength[2] = 0.0f;
  uniforms.sky_visibility_and_ambient_strength[3] = 0.0f;
  return uniforms;
}

} // namespace

bool run_composite_pass(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *color_texture,
    SDL_GPUTexture *position_texture, SDL_GPUTexture *voxel_texture,
    SDL_GPUTexture *material_texture, SDL_GPUTexture *composite_texture,
    const client_shader_pipelines &pipelines,
    const server_world_time_state &world_time,
    const octaryn_client_camera &camera,
    const octaryn_client_runtime_controls &controls, uint32_t target_width,
    uint32_t target_height, uint64_t frame_index,
    octaryn_client_frame_profile_sample *profile_sample) {
  if (pipelines.composite == nullptr || pipelines.atlas_sampler == nullptr) {
    log_line("live_composite_pass active=0 reason=missing_pipeline");
    return false;
  }

  octaryn_client_function_profile_scope composite_profile_scope(
      "composite_pass", frame_index, "sdl_gpu_commands");
  SDL_GPUStorageTextureReadWriteBinding write_texture{};
  write_texture.texture = composite_texture;
  write_texture.cycle = true;
  SDL_GPUComputePass *compute_pass =
      SDL_BeginGPUComputePass(command_buffer, &write_texture, 1u, nullptr, 0u);
  if (compute_pass == nullptr) {
    log_line("live_composite_pass active=0 reason=begin_failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding read_samplers[4]{};
  read_samplers[0].texture = color_texture;
  read_samplers[0].sampler = pipelines.atlas_sampler;
  read_samplers[1].texture = position_texture;
  read_samplers[1].sampler = pipelines.atlas_sampler;
  read_samplers[2].texture = voxel_texture;
  read_samplers[2].sampler = pipelines.atlas_sampler;
  read_samplers[3].texture = material_texture;
  read_samplers[3].sampler = pipelines.atlas_sampler;
  const composite_uniforms uniforms =
      build_composite_uniforms(world_time, camera, controls);

  const uint64_t composite_start = SDL_GetTicksNS();
  SDL_PushGPUDebugGroup(command_buffer, "composite");
  SDL_BindGPUComputePipeline(compute_pass, pipelines.composite);
  SDL_BindGPUComputeSamplers(compute_pass, 0u, read_samplers, 4u);
  SDL_PushGPUComputeUniformData(command_buffer, 0u, &uniforms,
                                sizeof(uniforms));
  SDL_DispatchGPUCompute(compute_pass, (target_width + 7u) / 8u,
                         (target_height + 7u) / 8u, 1u);
  SDL_EndGPUComputePass(compute_pass);
  SDL_PopGPUDebugGroup(command_buffer);

  if (profile_sample != nullptr) {
    profile_sample->composite_ms =
        octaryn_client_frame_profile_elapsed_ms_since(composite_start);
    profile_sample->post_ms =
        profile_sample->composite_ms + profile_sample->depth_ms;
  }
  if (!g_composite_path_logged && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_composite_pass active=1 "
                 "source=old_architecture_compute_shader target=(%" PRIu32
                 ",%" PRIu32 ") sky_visibility=%.6f ambient_strength=%.6f\n",
                 target_width, target_height,
                 uniforms.sky_visibility_and_ambient_strength[0],
                 uniforms.sky_visibility_and_ambient_strength[1]);
    std::fflush(g_log);
    g_composite_path_logged = true;
  }
  return true;
}

bool present_composite_to_swapchain(SDL_GPUCommandBuffer *command_buffer,
                                    SDL_GPUTexture *composite_texture,
                                    SDL_GPUTexture *swapchain_texture,
                                    const client_shader_pipelines &pipelines,
                                    uint64_t frame_index) {
  if (pipelines.present == nullptr || pipelines.atlas_sampler == nullptr) {
    log_line("live_present_pass active=0 reason=missing_pipeline");
    return false;
  }

  octaryn_client_function_profile_scope present_profile_scope(
      "present_pass", frame_index, "sdl_gpu_commands");
  SDL_GPUColorTargetInfo color_target{};
  color_target.texture = swapchain_texture;
  color_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
  color_target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &color_target, 1u, nullptr);
  if (render_pass == nullptr) {
    log_line("live_present_pass active=0 reason=begin_failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding bindings[2]{};
  bindings[0].texture = composite_texture;
  bindings[0].sampler = pipelines.atlas_sampler;
  bindings[1].texture = composite_texture;
  bindings[1].sampler = pipelines.atlas_sampler;
  const uint32_t overlay_enabled = 0u;
  SDL_BindGPUGraphicsPipeline(render_pass, pipelines.present);
  SDL_BindGPUFragmentSamplers(render_pass, 0u, bindings, 2u);
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &overlay_enabled,
                                 sizeof(overlay_enabled));
  SDL_DrawGPUPrimitives(render_pass, 3u, 1u, 0u, 0u);
  SDL_EndGPURenderPass(render_pass);

  if (!g_present_path_logged && g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_present_pass active=1 source=old_architecture_present_shader "
        "tone_map=1 linear_to_srgb=1\n");
    std::fflush(g_log);
    g_present_path_logged = true;
  }
  return true;
}

} // namespace octaryn_client_app
