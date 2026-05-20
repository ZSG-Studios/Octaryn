#pragma once

#include "Camera.h"
#include "Input.h"
#include "RuntimeControls.h"
#include "ShaderPipelines.h"

#include <SDL3/SDL.h>

#include <cstdint>

namespace octaryn_client_app {

camera build_player_render_camera(const camera &player_camera,
                                  const runtime_controls &controls);

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
                         uint64_t frame_index);

} // namespace octaryn_client_app
