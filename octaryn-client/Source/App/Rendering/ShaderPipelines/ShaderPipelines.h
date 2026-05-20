#pragma once

#include <SDL3/SDL.h>

namespace octaryn_client_app {

struct client_shader_pipelines {
  SDL_GPUGraphicsPipeline *sky = nullptr;
  SDL_GPUGraphicsPipeline *world = nullptr;
  SDL_GPUGraphicsPipeline *transparent = nullptr;
  SDL_GPUGraphicsPipeline *opaque_sprite = nullptr;
  SDL_GPUGraphicsPipeline *player_model = nullptr;
  SDL_GPUGraphicsPipeline *present = nullptr;
  SDL_GPUComputePipeline *composite = nullptr;
  SDL_GPUComputePipeline *ui = nullptr;
  SDL_GPUSampler *atlas_sampler = nullptr;
  SDL_GPUSampler *nearest_sampler = nullptr;
};

bool initialize_shader_pipelines(SDL_GPUDevice *device, SDL_Window *window,
                                 client_shader_pipelines &pipelines);
void release_shader_pipelines(SDL_GPUDevice *device,
                              client_shader_pipelines &pipelines);

} // namespace octaryn_client_app
