#include "FrameTargets.h"

#include "Log.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>

namespace octaryn_client_app {
namespace {

constexpr Uint8 kClearRed = 18;
constexpr Uint8 kClearGreen = 43;
constexpr Uint8 kClearBlue = 49;
constexpr Uint8 kClearAlpha = 255;

} // namespace

bool clear_gpu_swapchain(SDL_GPUCommandBuffer *command_buffer,
                         SDL_GPUTexture *swapchain_texture) {
  SDL_GPUColorTargetInfo target{};
  target.texture = swapchain_texture;
  target.clear_color = {static_cast<float>(kClearRed) / 255.0f,
                        static_cast<float>(kClearGreen) / 255.0f,
                        static_cast<float>(kClearBlue) / 255.0f,
                        static_cast<float>(kClearAlpha) / 255.0f};
  target.load_op = SDL_GPU_LOADOP_CLEAR;
  target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1u, nullptr);
  if (render_pass == nullptr) {
    log_line("gpu_clear_pass=failed");
    return false;
  }
  SDL_EndGPURenderPass(render_pass);
  return true;
}

bool begin_sky_pixel_readback(SDL_GPUDevice *device,
                              SDL_GPUCommandBuffer *command_buffer,
                              SDL_GPUTexture *source_texture,
                              SDL_GPUTextureFormat swapchain_format,
                              uint32_t target_width, uint32_t target_height,
                              gpu_pixel_readback &readback) {
  const Uint32 texel_size =
      SDL_GPUTextureFormatTexelBlockSize(swapchain_format);
  if (texel_size != 4u && texel_size != 8u) {
    log_line("live_sky_pixel active=0 source=gpu_readback "
             "reason=unsupported_format");
    return false;
  }

  readback.x = target_width > 8u ? target_width - 8u : 0u;
  readback.y = target_height > 8u ? 8u : 0u;
  readback.row_pitch = target_width * texel_size;
  readback.texel_size = texel_size;
  readback.width = target_width;
  readback.height = target_height;
  const Uint32 transfer_size = readback.row_pitch * target_height;

  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  transfer_info.size = transfer_size;
  readback.transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (readback.transfer == nullptr) {
    log_line(
        "live_sky_pixel active=0 source=gpu_readback reason=create_failed");
    return false;
  }

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  if (copy_pass == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
    readback.transfer = nullptr;
    log_line(
        "live_sky_pixel active=0 source=gpu_readback reason=copy_pass_failed");
    return false;
  }

  SDL_GPUTextureRegion source{};
  source.texture = source_texture;
  source.w = target_width;
  source.h = target_height;
  source.d = 1u;

  SDL_GPUTextureTransferInfo destination{};
  destination.transfer_buffer = readback.transfer;
  destination.pixels_per_row = target_width;
  destination.rows_per_layer = target_height;
  SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
  SDL_EndGPUCopyPass(copy_pass);
  return true;
}

bool finish_sky_pixel_readback(SDL_GPUDevice *device,
                               gpu_pixel_readback &readback) {
  if (readback.transfer == nullptr) {
    return false;
  }

  const void *mapped =
      SDL_MapGPUTransferBuffer(device, readback.transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
    readback.transfer = nullptr;
    log_line("live_sky_pixel active=0 source=gpu_readback reason=map_failed");
    return false;
  }

  const auto *bytes = static_cast<const uint8_t *>(mapped);
  const uint8_t *pixel = bytes + readback.y * readback.row_pitch +
                         readback.x * readback.texel_size;
  if (readback.texel_size == 8u) {
    const auto *half_pixel = reinterpret_cast<const uint16_t *>(pixel);
    const bool nonzero = half_pixel[0] != 0u || half_pixel[1] != 0u ||
                         half_pixel[2] != 0u || half_pixel[3] != 0u;
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_sky_pixel active=%d source=gpu_readback x=%" PRIu32
                   " y=%" PRIu32 " raw16=(%u,%u,%u,%u)\n",
                   nonzero ? 1 : 0, readback.x, readback.y,
                   static_cast<unsigned>(half_pixel[0]),
                   static_cast<unsigned>(half_pixel[1]),
                   static_cast<unsigned>(half_pixel[2]),
                   static_cast<unsigned>(half_pixel[3]));
      std::fflush(g_log);
    }
    SDL_UnmapGPUTransferBuffer(device, readback.transfer);
    SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
    readback.transfer = nullptr;
    return nonzero;
  }

  const bool clear_rgba = pixel[0] == kClearRed && pixel[1] == kClearGreen &&
                          pixel[2] == kClearBlue && pixel[3] == kClearAlpha;
  const bool clear_bgra = pixel[0] == kClearBlue && pixel[1] == kClearGreen &&
                          pixel[2] == kClearRed && pixel[3] == kClearAlpha;
  const bool clear_match = clear_rgba || clear_bgra;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_sky_pixel active=%d source=gpu_readback x=%" PRIu32
                 " y=%" PRIu32 " raw=(%u,%u,%u,%u) clear_match=%d\n",
                 clear_match ? 0 : 1, readback.x, readback.y,
                 static_cast<unsigned>(pixel[0]),
                 static_cast<unsigned>(pixel[1]),
                 static_cast<unsigned>(pixel[2]),
                 static_cast<unsigned>(pixel[3]), clear_match ? 1 : 0);
    std::fflush(g_log);
  }

  SDL_UnmapGPUTransferBuffer(device, readback.transfer);
  SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
  readback.transfer = nullptr;
  return !clear_match;
}

bool finish_terrain_visual_readback(SDL_GPUDevice *device,
                                    gpu_pixel_readback &readback,
                                    uint64_t frame_index) {
  if (readback.transfer == nullptr) {
    return false;
  }

  const void *mapped =
      SDL_MapGPUTransferBuffer(device, readback.transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
    readback.transfer = nullptr;
    log_line("live_terrain_visual_audit active=0 reason=map_failed");
    return false;
  }

  const auto *bytes = static_cast<const uint8_t *>(mapped);
  constexpr Uint32 sample_x[] = {38u, 50u, 62u};
  constexpr Uint32 sample_y[] = {55u, 65u, 75u};
  uint32_t sky_like = 0u;
  uint32_t samples = 0u;
  if (readback.texel_size == 8u && readback.width != 0u &&
      readback.height != 0u) {
    for (Uint32 y_percent : sample_y) {
      for (Uint32 x_percent : sample_x) {
        const Uint32 x = readback.width * x_percent / 100u;
        const Uint32 y = readback.height * y_percent / 100u;
        const uint8_t *pixel =
            bytes + y * readback.row_pitch + x * readback.texel_size;
        const auto *half_pixel = reinterpret_cast<const uint16_t *>(pixel);
        const bool sky_pixel = half_pixel[0] >= 12000u &&
                               half_pixel[1] >= 14000u &&
                               half_pixel[2] >= 15500u;
        sky_like += sky_pixel ? 1u : 0u;
        ++samples;
      }
    }
  }
  const bool hole = samples != 0u && sky_like >= 3u;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_terrain_visual_audit active=1 frame=%" PRIu64
                 " samples=%" PRIu32 " sky_like=%" PRIu32 " hole=%d\n",
                 frame_index, samples, sky_like, hole ? 1 : 0);
    std::fflush(g_log);
  }

  SDL_UnmapGPUTransferBuffer(device, readback.transfer);
  SDL_ReleaseGPUTransferBuffer(device, readback.transfer);
  readback.transfer = nullptr;
  return true;
}

SDL_GPUTexture *create_frame_color_target(SDL_GPUDevice *device,
                                          SDL_GPUTextureFormat format,
                                          uint32_t width, uint32_t height) {
  SDL_GPUTextureCreateInfo texture_info{};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format = format;
  texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                       SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
                       SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width = width;
  texture_info.height = height;
  texture_info.layer_count_or_depth = 1u;
  texture_info.num_levels = 1u;
  texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  return SDL_CreateGPUTexture(device, &texture_info);
}

SDL_GPUTexture *create_composite_frame_texture(SDL_GPUDevice *device,
                                               SDL_GPUTextureFormat format,
                                               uint32_t width,
                                               uint32_t height) {
  SDL_GPUTextureCreateInfo texture_info{};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format = format;
  texture_info.usage =
      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER |
      SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE |
      SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
  texture_info.width = width;
  texture_info.height = height;
  texture_info.layer_count_or_depth = 1u;
  texture_info.num_levels = 1u;
  texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  return SDL_CreateGPUTexture(device, &texture_info);
}

void release_frame_render_targets(SDL_GPUDevice *device,
                                  frame_render_targets &targets) {
  if (targets.frame != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.frame);
  }
  if (targets.color != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.color);
  }
  if (targets.depth != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.depth);
  }
  if (targets.position != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.position);
  }
  if (targets.voxel != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.voxel);
  }
  if (targets.material != nullptr) {
    SDL_ReleaseGPUTexture(device, targets.material);
  }
  targets = {};
}

bool ensure_frame_render_targets(SDL_GPUDevice *device,
                                 SDL_GPUTextureFormat color_format,
                                 uint32_t width, uint32_t height,
                                 frame_render_targets &targets) {
  if (targets.frame != nullptr && targets.color_format == color_format &&
      targets.width == width && targets.height == height) {
    return true;
  }

  release_frame_render_targets(device, targets);
  targets.color_format = color_format;
  targets.width = width;
  targets.height = height;
  targets.frame =
      create_composite_frame_texture(device, color_format, width, height);
  targets.color = create_frame_color_target(device, color_format, width, height);
  targets.position = create_frame_color_target(
      device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, width, height);
  targets.voxel =
      create_frame_color_target(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                width, height);
  targets.material =
      create_frame_color_target(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                width, height);

  SDL_GPUTextureCreateInfo depth_info{};
  depth_info.type = SDL_GPU_TEXTURETYPE_2D;
  depth_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_info.width = width;
  depth_info.height = height;
  depth_info.layer_count_or_depth = 1u;
  depth_info.num_levels = 1u;
  depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  targets.depth = SDL_CreateGPUTexture(device, &depth_info);

  if (targets.frame == nullptr || targets.color == nullptr ||
      targets.depth == nullptr || targets.position == nullptr ||
      targets.voxel == nullptr || targets.material == nullptr) {
    log_line("gpu_frame_targets=create_failed");
    release_frame_render_targets(device, targets);
    return false;
  }
  return true;
}

} // namespace octaryn_client_app
