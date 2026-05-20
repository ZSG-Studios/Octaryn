#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

namespace octaryn_client_app {

struct gpu_pixel_readback {
  SDL_GPUTransferBuffer *transfer = nullptr;
  Uint32 row_pitch = 0u;
  Uint32 texel_size = 0u;
  Uint32 width = 0u;
  Uint32 height = 0u;
  Uint32 x = 0u;
  Uint32 y = 0u;
};

struct frame_render_targets {
  SDL_GPUTexture *frame = nullptr;
  SDL_GPUTexture *color = nullptr;
  SDL_GPUTexture *depth = nullptr;
  SDL_GPUTexture *position = nullptr;
  SDL_GPUTexture *voxel = nullptr;
  SDL_GPUTexture *material = nullptr;
  SDL_GPUTextureFormat color_format = SDL_GPU_TEXTUREFORMAT_INVALID;
  uint32_t width = 0u;
  uint32_t height = 0u;
};

bool clear_gpu_swapchain(SDL_GPUCommandBuffer *command_buffer,
                         SDL_GPUTexture *swapchain_texture);
bool begin_sky_pixel_readback(SDL_GPUDevice *device,
                              SDL_GPUCommandBuffer *command_buffer,
                              SDL_GPUTexture *source_texture,
                              SDL_GPUTextureFormat swapchain_format,
                              uint32_t target_width, uint32_t target_height,
                              gpu_pixel_readback &readback);
bool finish_sky_pixel_readback(SDL_GPUDevice *device,
                               gpu_pixel_readback &readback);
bool finish_terrain_visual_readback(SDL_GPUDevice *device,
                                    gpu_pixel_readback &readback,
                                    uint64_t frame_index);
SDL_GPUTexture *create_frame_color_target(SDL_GPUDevice *device,
                                          SDL_GPUTextureFormat format,
                                          uint32_t width, uint32_t height);
SDL_GPUTexture *create_composite_frame_texture(SDL_GPUDevice *device,
                                               SDL_GPUTextureFormat format,
                                               uint32_t width,
                                               uint32_t height);
bool ensure_frame_render_targets(SDL_GPUDevice *device,
                                 SDL_GPUTextureFormat color_format,
                                 uint32_t width, uint32_t height,
                                 frame_render_targets &targets);
void release_frame_render_targets(SDL_GPUDevice *device,
                                  frame_render_targets &targets);

} // namespace octaryn_client_app
