#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

namespace octaryn_client_app {

struct gpu_pixel_readback {
  SDL_GPUTransferBuffer *transfer = nullptr;
  Uint32 row_pitch = 0u;
  Uint32 texel_size = 0u;
  Uint32 x = 0u;
  Uint32 y = 0u;
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
SDL_GPUTexture *create_frame_color_target(SDL_GPUDevice *device,
                                          SDL_GPUTextureFormat format,
                                          uint32_t width, uint32_t height);
SDL_GPUTexture *create_composite_frame_texture(SDL_GPUDevice *device,
                                               SDL_GPUTextureFormat format,
                                               uint32_t width,
                                               uint32_t height);

} // namespace octaryn_client_app
