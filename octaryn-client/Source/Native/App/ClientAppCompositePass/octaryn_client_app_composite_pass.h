#pragma once

#include "octaryn_client_app_shader_pipelines.h"
#include "octaryn_client_app_world_stream.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_frame_profile.h"
#include "octaryn_client_runtime_controls.h"

#include <SDL3/SDL.h>
#include <cstdint>

namespace octaryn_client_app {

bool run_composite_pass(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *color_texture,
    SDL_GPUTexture *position_texture, SDL_GPUTexture *voxel_texture,
    SDL_GPUTexture *material_texture, SDL_GPUTexture *composite_texture,
    const client_shader_pipelines &pipelines,
    const server_world_time_state &world_time,
    const octaryn_client_camera &camera,
    const octaryn_client_runtime_controls &controls, uint32_t target_width,
    uint32_t target_height, uint64_t frame_index,
    octaryn_client_frame_profile_sample *profile_sample);
bool present_composite_to_swapchain(SDL_GPUCommandBuffer *command_buffer,
                                    SDL_GPUTexture *composite_texture,
                                    SDL_GPUTexture *swapchain_texture,
                                    const client_shader_pipelines &pipelines,
                                    uint64_t frame_index);

} // namespace octaryn_client_app
