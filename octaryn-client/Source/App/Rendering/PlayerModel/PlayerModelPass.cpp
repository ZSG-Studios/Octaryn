#include "PlayerModelPass.h"

#include "Log.h"
#include "PlayerModelAsset.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace octaryn_client_app {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kThirdPersonCameraDistance = 5.0f;
constexpr uint32_t kPlayerModelBatchVertices = 63u;

struct matrix_uniform {
  float values[4][4]{};
};

struct camera_uniforms {
  float position[4]{};
};

struct player_vertex_uniforms {
  player_model_render_vertex vertices[64]{};
};

matrix_uniform matrix_from_camera_values(const float values[4][4]) {
  matrix_uniform output{};
  std::memcpy(output.values, values, sizeof(output.values));
  return output;
}

camera_uniforms camera_uniform_from_camera(const camera &camera) {
  camera_uniforms output{};
  output.position[0] = camera.position[0];
  output.position[1] = camera.position[1];
  output.position[2] = camera.position[2];
  output.position[3] = 1.0f;
  return output;
}

const char *camera_mode_name(const runtime_controls &controls) {
  if (controls.camera_mode == 1u) {
    return "third_person_back";
  }
  if (controls.camera_mode == 2u) {
    return "third_person_front";
  }
  return "first_person";
}

void camera_forward(const camera &camera, float &x, float &y, float &z) {
  const float pitch_cos = std::cos(camera.pitch_radians);
  x = std::sin(camera.yaw_radians) * pitch_cos;
  y = std::sin(camera.pitch_radians);
  z = -std::cos(camera.yaw_radians) * pitch_cos;
}

} // namespace

camera build_player_render_camera(const camera &player_camera,
                                  const runtime_controls &controls) {
  camera output = player_camera;
  if (controls.camera_mode == 1u || controls.camera_mode == 2u) {
    float forward_x = 0.0f;
    float forward_y = 0.0f;
    float forward_z = 0.0f;
    camera_forward(player_camera, forward_x, forward_y, forward_z);
    const float side = controls.camera_mode == 1u ? -1.0f : 1.0f;
    output.position[0] += side * forward_x * kThirdPersonCameraDistance;
    output.position[1] += side * forward_y * kThirdPersonCameraDistance;
    output.position[2] += side * forward_z * kThirdPersonCameraDistance;
    if (controls.camera_mode == 2u) {
      output.yaw_radians += kPi;
      output.pitch_radians = -output.pitch_radians;
    }
    camera_update(&output);
  }
  return output;
}

bool render_player_model(SDL_GPUCommandBuffer *command_buffer,
                         SDL_GPUTexture *target_texture,
                         SDL_GPUTexture *depth_texture,
                         SDL_GPUTexture *position_texture,
                         SDL_GPUTexture *voxel_texture,
                         SDL_GPUTexture *material_texture,
                         const client_shader_pipelines &pipelines,
                         const camera &player_camera,
                         const camera &render_camera,
                         const client_input_debug_state &input,
                         const runtime_controls &controls,
                         uint64_t frame_index) {
  if (controls.session_active == 0u || pipelines.player_model == nullptr) {
    return true;
  }

  if (controls.camera_mode == 0u) {
    if (g_log != nullptr && frame_index % 120u == 0u) {
      player_model_frame_vertices hidden_frame{};
      const bool built = build_player_model_frame(
          player_camera, input, controls, frame_index, hidden_frame);
      std::fprintf(g_log,
                   "live_player_model frame=%" PRIu64
                   " active=0 camera=first_person reason=body_hidden "
                   "animation=%s vertices=%u loader=fastgltf animator=ozz "
                   "skinner=ozz_geometry skinned=%d blended=%d\n",
                   frame_index, built ? hidden_frame.animation_name : "none",
                   built ? hidden_frame.count : 0u, built ? 1 : 0,
                   built ? 1 : 0);
      std::fflush(g_log);
    }
    return true;
  }

  player_model_frame_vertices frame{};
  if (!build_player_model_frame(player_camera, input, controls, frame_index,
                                frame) ||
      frame.count == 0u) {
    return true;
  }

  SDL_GPUColorTargetInfo targets[4]{};
  targets[0].texture = target_texture;
  targets[1].texture = position_texture;
  targets[2].texture = voxel_texture;
  targets[3].texture = material_texture;
  for (auto &target : targets) {
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
  }

  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = depth_texture;
  depth.load_op = SDL_GPU_LOADOP_LOAD;
  depth.store_op = SDL_GPU_STOREOP_STORE;
  depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  SDL_GPURenderPass *pass =
      SDL_BeginGPURenderPass(command_buffer, targets, 4u, &depth);
  if (pass == nullptr) {
    log_line("live_player_model_render_pass=failed");
    return false;
  }

  const matrix_uniform projection =
      matrix_from_camera_values(render_camera.projection);
  const matrix_uniform view = matrix_from_camera_values(render_camera.view);
  const camera_uniforms camera_uniform =
      camera_uniform_from_camera(render_camera);
  SDL_PushGPUVertexUniformData(command_buffer, 0u, &projection,
                               sizeof(projection));
  SDL_PushGPUVertexUniformData(command_buffer, 1u, &view, sizeof(view));
  SDL_PushGPUVertexUniformData(command_buffer, 2u, &camera_uniform,
                               sizeof(camera_uniform));
  SDL_BindGPUGraphicsPipeline(pass, pipelines.player_model);
  for (uint32_t offset = 0u; offset < frame.count;) {
    const uint32_t remaining = frame.count - offset;
    const uint32_t batch_count =
        remaining > kPlayerModelBatchVertices
            ? kPlayerModelBatchVertices
            : remaining;
    player_vertex_uniforms uniforms{};
    std::memcpy(uniforms.vertices, frame.vertices.data() + offset,
                sizeof(player_model_render_vertex) * batch_count);
    SDL_PushGPUVertexUniformData(command_buffer, 3u, &uniforms,
                                 sizeof(uniforms));
    SDL_DrawGPUPrimitives(pass, batch_count, 1u, 0u, 0u);
    offset += batch_count;
  }
  SDL_EndGPURenderPass(pass);

  if (g_log != nullptr && frame_index % 120u == 0u) {
    std::fprintf(g_log,
                 "live_player_model frame=%" PRIu64
                 " active=1 camera=%s animation=%s vertices=%u "
                 "loader=fastgltf animator=ozz skinner=ozz_geometry "
                 "skinned=1 blended=1\n",
                 frame_index, camera_mode_name(controls), frame.animation_name,
                 frame.count);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
