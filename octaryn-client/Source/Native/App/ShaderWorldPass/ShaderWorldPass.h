#pragma once

#include "BlockInteraction.h"
#include "ShaderPipelines.h"
#include "WorldStream.h"
#include "BlockAtlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_frame_profile.h"
#include "RuntimeControls.h"
#include "octaryn_client_world_mesh_upload.h"

#include <SDL3/SDL.h>
#include <cstdint>

namespace octaryn_client_app {

bool draw_shader_world(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    SDL_GPUTexture *depth_texture, SDL_GPUTexture *position_texture,
    SDL_GPUTexture *voxel_texture, SDL_GPUTexture *material_texture,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const octaryn_client_camera &camera,
    const client_block_raycast_hit &selection_hit,
    const server_world_time_state &world_time,
    const runtime_controls &controls, uint64_t frame_index,
    octaryn_client_frame_profile_sample *profile_sample);

} // namespace octaryn_client_app
