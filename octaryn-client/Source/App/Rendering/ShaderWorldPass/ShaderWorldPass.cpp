#include "ShaderWorldPass.h"

#include "Log.h"
#include "SkyUniforms.h"
#include "FunctionProfile.h"
#include "VisibleSectionTraversal.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace octaryn_client_app {
namespace {

constexpr uint32_t kDrawFlagUseFaceBuffer = 1u << 1u;
constexpr uint32_t kDrawFlagIndirect = 1u << 5u;

struct matrix_uniform {
  float values[4][4]{};
};

struct chunk_uniforms {
  int32_t chunk_position[4]{};
  uint32_t face_offset = 0u;
  uint32_t draw_flags = 0u;
};

struct camera_uniforms {
  float position[4]{};
};

struct opaque_fragment_uniforms {
  float skylight_floor = 0.0f;
  float cloud_time_seconds = 0.0f;
  float sky_visibility = 1.0f;
  float twilight_strength = 0.0f;
  float celestial_visibility = 1.0f;
  uint32_t material_flags = 0u;
  uint32_t pad0 = 0u;
  uint32_t pad1 = 0u;
  float camera_position[4]{};
};

struct hidden_block_uniforms {
  uint32_t hidden_block_count = 0u;
  int32_t hidden_pad[3]{};
  int32_t hidden_blocks[32][4]{};
};

struct block_highlight_uniforms {
  uint32_t highlight_block_enabled = 0u;
  int32_t highlight_pad[3]{};
  int32_t highlight_block[4]{};
};

bool g_sky_path_logged;

matrix_uniform matrix_from_camera_values(const float values[4][4]) {
  matrix_uniform output{};
  std::memcpy(output.values, values, sizeof(output.values));
  return output;
}

camera_uniforms
camera_uniform_from_camera(const camera &camera) {
  camera_uniforms uniforms{};
  uniforms.position[0] = camera.position[0];
  uniforms.position[1] = camera.position[1];
  uniforms.position[2] = camera.position[2];
  uniforms.position[3] = 1.0f;
  return uniforms;
}

} // namespace

bool draw_shader_world(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    SDL_GPUTexture *depth_texture, SDL_GPUTexture *position_texture,
    SDL_GPUTexture *voxel_texture, SDL_GPUTexture *material_texture,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const camera &camera,
    const client_block_raycast_hit &selection_hit,
    const server_world_time_state &world_time,
    const runtime_controls &controls, uint64_t frame_index,
    frame_profile_sample *profile_sample) {
  (void)mesh_frame;
  if (pipelines.sky == nullptr) {
    return true;
  }

  function_profile_scope sky_profile_scope(
      "sky_pass", frame_index, "sdl_gpu_commands");
  const uint64_t sky_start = SDL_GetTicksNS();
  SDL_GPUColorTargetInfo target{};
  target.texture = target_texture;
  target.load_op = SDL_GPU_LOADOP_LOAD;
  target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *sky_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1u, nullptr);
  if (sky_pass == nullptr) {
    log_line("live_sky_render_pass=failed");
    return false;
  }

  const matrix_uniform projection =
      matrix_from_camera_values(camera.projection);
  const matrix_uniform view = matrix_from_camera_values(camera.view);
  const sky_uniforms sky = build_sky_uniforms(world_time, camera, controls);
  const camera_uniforms camera_uniform = camera_uniform_from_camera(camera);

  SDL_PushGPUVertexUniformData(command_buffer, 0u, &projection,
                               sizeof(projection));
  SDL_PushGPUVertexUniformData(command_buffer, 1u, &view, sizeof(view));
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &sky, sizeof(sky));
  SDL_BindGPUGraphicsPipeline(sky_pass, pipelines.sky);
  SDL_DrawGPUPrimitives(sky_pass, 36u, 1u, 0u, 0u);
  SDL_EndGPURenderPass(sky_pass);
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_sky_ms =
        frame_profile_elapsed_ms_since(sky_start);
  }
  if (!g_sky_path_logged && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_sky_pass active=1 source=server_world_time "
                 "day_fraction=%.6f total_seconds=%.3f\n",
                 world_time.day_fraction, world_time.total_seconds);
    std::fflush(g_log);
    g_sky_path_logged = true;
  }

  const bool world_ready =
      pipelines.world != nullptr && pipelines.atlas_sampler != nullptr &&
      atlas.color_texture != nullptr && atlas.normal_texture != nullptr &&
      atlas.specular_texture != nullptr &&
      world_mesh_gpu_has_geometry(mesh_buffers);
  if (!world_ready) {
    return true;
  }

  SDL_GPUColorTargetInfo world_targets[4]{};
  world_targets[0].texture = target_texture;
  world_targets[0].load_op = SDL_GPU_LOADOP_LOAD;
  world_targets[0].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[1].texture = position_texture;
  world_targets[1].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[1].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[2].texture = voxel_texture;
  world_targets[2].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[2].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[3].texture = material_texture;
  world_targets[3].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[3].store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = depth_texture;
  depth.load_op = SDL_GPU_LOADOP_CLEAR;
  depth.store_op = SDL_GPU_STOREOP_STORE;
  depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth.clear_depth = 0.0f;
  SDL_GPURenderPass *world_pass =
      SDL_BeginGPURenderPass(command_buffer, world_targets, 4u, &depth);
  if (world_pass == nullptr) {
    log_line("live_world_render_pass=failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding atlas_bindings[3]{};
  atlas_bindings[0].texture = atlas.color_texture;
  atlas_bindings[0].sampler = pipelines.atlas_sampler;
  atlas_bindings[1].texture = atlas.normal_texture;
  atlas_bindings[1].sampler = pipelines.atlas_sampler;
  atlas_bindings[2].texture = atlas.specular_texture;
  atlas_bindings[2].sampler = pipelines.atlas_sampler;
  SDL_BindGPUFragmentSamplers(world_pass, 0u, atlas_bindings, 3u);
  SDL_PushGPUVertexUniformData(command_buffer, 3u, &camera_uniform,
                               sizeof(camera_uniform));
  opaque_fragment_uniforms fragment_uniforms{};
  fragment_uniforms.skylight_floor = 0.24f;
  fragment_uniforms.cloud_time_seconds =
      static_cast<float>(std::fmod(world_time.total_seconds, 86400.0));
  fragment_uniforms.sky_visibility = sky.light_direction_and_sky_visibility[3];
  fragment_uniforms.twilight_strength = sky.twilight_celestial_cloud_time[0];
  fragment_uniforms.celestial_visibility = sky.twilight_celestial_cloud_time[1];
  fragment_uniforms.material_flags = (controls.pbr_enabled != 0u ? 0x1u : 0u) |
                                     (controls.pom_enabled != 0u ? 0x2u : 0u);
  fragment_uniforms.camera_position[0] = camera.position[0];
  fragment_uniforms.camera_position[1] = camera.position[1];
  fragment_uniforms.camera_position[2] = camera.position[2];
  fragment_uniforms.camera_position[3] = 1.0f;
  hidden_block_uniforms hidden_uniforms{};
  block_highlight_uniforms highlight_uniforms{};
  if (selection_hit.has_hit) {
    highlight_uniforms.highlight_block_enabled = 1u;
    highlight_uniforms.highlight_block[0] = selection_hit.hit.x;
    highlight_uniforms.highlight_block[1] = selection_hit.hit.y;
    highlight_uniforms.highlight_block[2] = selection_hit.hit.z;
    highlight_uniforms.highlight_block[3] = selection_hit.block;
  }
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &fragment_uniforms,
                                 sizeof(fragment_uniforms));
  SDL_PushGPUFragmentUniformData(command_buffer, 1u, &hidden_uniforms,
                                 sizeof(hidden_uniforms));
  SDL_PushGPUFragmentUniformData(command_buffer, 2u, &highlight_uniforms,
                                 sizeof(highlight_uniforms));
  SDL_BindGPUGraphicsPipeline(world_pass, pipelines.world);

  const uint64_t traversal_start = SDL_GetTicksNS();
  const visible_section_draw_list visible_sections =
      build_visible_section_draw_list(mesh_buffers, camera);
  const float traversal_ms = frame_profile_elapsed_ms_since(traversal_start);
  const uint64_t opaque_start = SDL_GetTicksNS();
  uint32_t drawn_chunks = 0u;
  uint64_t drawn_faces = 0u;
  for (const size_t chunk_index : visible_sections.opaque_indices) {
    const world_mesh_gpu_buffers::chunk_buffers &gpu_chunk =
        mesh_buffers.chunks[chunk_index];
    const octaryn_client_chunk_mesh_upload_record &chunk = gpu_chunk.record;
    if (chunk.opaque_face_count == 0u || gpu_chunk.opaque_faces == nullptr) {
      continue;
    }

    SDL_GPUBuffer *storage_buffers[2] = {
        gpu_chunk.opaque_faces,
        gpu_chunk.opaque_faces,
    };
    SDL_BindGPUVertexStorageBuffers(world_pass, 0u, storage_buffers, 2u);
    chunk_uniforms chunk_uniform{};
    chunk_uniform.chunk_position[0] = chunk.chunk_x * 32;
    chunk_uniform.chunk_position[1] = chunk.chunk_y * 32;
    chunk_uniform.chunk_position[2] = chunk.chunk_z * 32;
    chunk_uniform.chunk_position[3] = 0;
    chunk_uniform.face_offset = 0u;
    chunk_uniform.draw_flags = kDrawFlagUseFaceBuffer | kDrawFlagIndirect;
    SDL_PushGPUVertexUniformData(command_buffer, 2u, &chunk_uniform,
                                 sizeof(chunk_uniform));
    if (gpu_chunk.opaque_indirect != nullptr) {
      SDL_DrawGPUPrimitivesIndirect(world_pass, gpu_chunk.opaque_indirect, 0u,
                                    1u);
    } else {
      SDL_DrawGPUPrimitives(world_pass, chunk.opaque_face_count * 6u, 1u, 0u,
                            0u);
    }
    ++drawn_chunks;
    drawn_faces += chunk.opaque_face_count;
  }
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_opaque_ms =
        frame_profile_elapsed_ms_since(opaque_start);
  }

  const uint64_t sprite_start = SDL_GetTicksNS();
  SDL_BindGPUGraphicsPipeline(world_pass, pipelines.opaque_sprite);

  uint32_t drawn_sprite_chunks = 0u;
  uint64_t drawn_sprite_indices = 0u;
  for (const size_t chunk_index : visible_sections.sprite_indices) {
    const world_mesh_gpu_buffers::chunk_buffers &gpu_chunk =
        mesh_buffers.chunks[chunk_index];
    const octaryn_client_chunk_mesh_upload_record &chunk = gpu_chunk.record;
    if (chunk.sprite_index_count == 0u ||
        gpu_chunk.sprite_vertices == nullptr) {
      continue;
    }

    SDL_GPUBuffer *sprite_storage_buffers[2] = {
        gpu_chunk.sprite_vertices,
        gpu_chunk.sprite_vertices,
    };
    SDL_BindGPUVertexStorageBuffers(world_pass, 0u, sprite_storage_buffers, 2u);
    chunk_uniforms chunk_uniform{};
    chunk_uniform.chunk_position[0] = chunk.chunk_x * 32;
    chunk_uniform.chunk_position[1] = chunk.chunk_y * 32;
    chunk_uniform.chunk_position[2] = chunk.chunk_z * 32;
    chunk_uniform.chunk_position[3] = 0;
    chunk_uniform.face_offset = 0u;
    chunk_uniform.draw_flags = kDrawFlagIndirect;
    SDL_PushGPUVertexUniformData(command_buffer, 2u, &chunk_uniform,
                                 sizeof(chunk_uniform));
    if (gpu_chunk.sprite_indirect != nullptr) {
      SDL_DrawGPUPrimitivesIndirect(world_pass, gpu_chunk.sprite_indirect, 0u,
                                    1u);
    } else {
      SDL_DrawGPUPrimitives(world_pass, chunk.sprite_index_count, 1u, 0u, 0u);
    }
    ++drawn_sprite_chunks;
    drawn_sprite_indices += chunk.sprite_index_count;
  }
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_sprite_ms =
        frame_profile_elapsed_ms_since(sprite_start);
  }

  SDL_EndGPURenderPass(world_pass);
  if (g_log != nullptr && (drawn_faces != 0u || drawn_sprite_indices != 0u)) {
    std::fprintf(g_log,
                 "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline "
                 "active=1 path=direct_indirect chunks=%" PRIu32
                 " opaque_faces=%" PRIu64
                 " sprite_chunks=%" PRIu32 " sprite_indices=%" PRIu64
                 " total_sections=%" PRIu32 " visited_sections=%" PRIu32
                 " frustum_rejected=%" PRIu32 " section_graph_fallback=%" PRIu32
                 " traversal_ms=%.3f"
                 "\n",
                 drawn_chunks, drawn_faces, drawn_sprite_chunks,
                 drawn_sprite_indices, visible_sections.total_sections,
                 visible_sections.visited_sections,
                 visible_sections.frustum_rejected, visible_sections.fallback,
                 traversal_ms);
    if (selection_hit.has_hit) {
      std::fprintf(g_log,
                   "live_block_highlight active=1 source=opaque_texture_shader "
                   "block=(%d,%d,%d,%u)\n",
                   selection_hit.hit.x, selection_hit.hit.y,
                   selection_hit.hit.z,
                   static_cast<unsigned>(selection_hit.block));
    }
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
