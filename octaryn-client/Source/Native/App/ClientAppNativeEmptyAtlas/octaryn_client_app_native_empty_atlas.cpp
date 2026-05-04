#include "octaryn_client_app_native_empty_atlas.h"

#include "octaryn_client_app_log.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace octaryn_client_app {

namespace {

using octaryn::client::rendering::destroy_client_block_atlas;

constexpr int32_t kNativeEmptyWorldAtlasTileSize = 16;

bool upload_solid_texture_array(SDL_GPUDevice *device, SDL_GPUTexture *texture,
                                const std::array<uint8_t, 4> &pixel,
                                const char *log_prefix) {
  const uint32_t tile_size =
      static_cast<uint32_t>(kNativeEmptyWorldAtlasTileSize);
  std::vector<uint8_t> pixels(tile_size * tile_size * 4u);
  for (size_t offset = 0u; offset < pixels.size(); offset += 4u) {
    pixels[offset + 0u] = pixel[0u];
    pixels[offset + 1u] = pixel[1u];
    pixels[offset + 2u] = pixel[2u];
    pixels[offset + 3u] = pixel[3u];
  }

  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = static_cast<Uint32>(pixels.size());
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_transfer=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_transfer=map_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }
  std::memcpy(mapped, pixels.data(), pixels.size());
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_command=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  if (copy_pass == nullptr) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_copy_pass=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = transfer;
  source.pixels_per_row = tile_size;
  source.rows_per_layer = tile_size;
  SDL_GPUTextureRegion destination{};
  destination.texture = texture;
  destination.layer = 0u;
  destination.w = tile_size;
  destination.h = tile_size;
  destination.d = 1u;
  SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
  SDL_EndGPUCopyPass(copy_pass);

  const bool submitted = SDL_SubmitGPUCommandBuffer(command_buffer);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  if (!submitted || !SDL_WaitForGPUIdle(device)) {
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_upload=failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }
  return true;
}

SDL_GPUTexture *create_native_empty_world_atlas_texture(
    SDL_GPUDevice *device, SDL_GPUTextureFormat format,
    const std::array<uint8_t, 4> &pixel, const char *log_prefix) {
  SDL_GPUTextureCreateInfo texture_info{};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
  texture_info.format = format;
  texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width = kNativeEmptyWorldAtlasTileSize;
  texture_info.height = kNativeEmptyWorldAtlasTileSize;
  texture_info.layer_count_or_depth = 1u;
  texture_info.num_levels = 1u;
  texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);
  if (texture == nullptr) {
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return nullptr;
  }
  if (!upload_solid_texture_array(device, texture, pixel, log_prefix)) {
    SDL_ReleaseGPUTexture(device, texture);
    return nullptr;
  }
  return texture;
}

} // namespace

bool load_native_empty_world_atlas(
    SDL_GPUDevice *device,
    octaryn::client::rendering::ClientBlockAtlas &atlas) {
  atlas.device = device;
  atlas.tile_size = kNativeEmptyWorldAtlasTileSize;
  atlas.layer_count = 1;
  atlas.animation_frames = 0;
  atlas.animation_count = 0;
  atlas.block_top_layers.assign(1u, 0);
  atlas.placeable_blocks.clear();

  atlas.color_texture = create_native_empty_world_atlas_texture(
      device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
      std::array<uint8_t, 4>{255u, 255u, 255u, 255u},
      "native_empty_atlas_texture");
  atlas.normal_texture = create_native_empty_world_atlas_texture(
      device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      std::array<uint8_t, 4>{128u, 128u, 255u, 255u},
      "native_empty_atlas_normal_texture");
  atlas.specular_texture = create_native_empty_world_atlas_texture(
      device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      std::array<uint8_t, 4>{0u, 0u, 0u, 0u},
      "native_empty_atlas_specular_texture");

  if (atlas.color_texture == nullptr || atlas.normal_texture == nullptr ||
      atlas.specular_texture == nullptr) {
    destroy_client_block_atlas(atlas);
    return false;
  }

  log_line("native_empty_atlas=loaded layers=1 tile_size=16 material=white");
  return true;
}

} // namespace octaryn_client_app
