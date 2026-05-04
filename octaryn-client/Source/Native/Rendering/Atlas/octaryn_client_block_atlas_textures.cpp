#include "octaryn_client_block_atlas_textures.h"

#include "octaryn_client_block_atlas_bundle_file.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <string>

namespace octaryn::client::rendering {

namespace {

bool surface_dimensions_match(const SDL_Surface *surface,
                              const ClientBlockAtlas &atlas,
                              ClientBlockAtlasTextureKind kind) {
  if (surface == nullptr) {
    return false;
  }

  if (kind == ClientBlockAtlasTextureKind::animation) {
    return surface->h == atlas.tile_size && surface->w >= atlas.tile_size &&
           surface->w % atlas.tile_size == 0;
  }

  const int32_t expected_width = atlas.layer_count * atlas.tile_size;
  return surface->w == expected_width && surface->h == atlas.tile_size;
}

bool upload_surface_to_gpu_texture(SDL_GPUDevice *device, FILE *log,
                                   SDL_Surface *surface, const char *log_prefix,
                                   SDL_GPUTexture *texture) {
  SDL_Surface *upload_surface =
      SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  if (upload_surface == nullptr) {
    if (log != nullptr) {
      std::fprintf(log, "%s_convert=failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  const Uint32 bytes_per_pixel = 4u;
  const Uint32 tile_size = static_cast<Uint32>(upload_surface->h);
  const Uint32 layer_count = static_cast<Uint32>(upload_surface->w) / tile_size;
  const Uint32 upload_size =
      static_cast<Uint32>(upload_surface->w * upload_surface->h) *
      bytes_per_pixel;
  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = upload_size;
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    SDL_DestroySurface(upload_surface);
    if (log != nullptr) {
      std::fprintf(log, "%s_transfer=create_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(upload_surface);
    if (log != nullptr) {
      std::fprintf(log, "%s_transfer=map_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  const Uint8 *source = static_cast<const Uint8 *>(upload_surface->pixels);
  Uint8 *destination = static_cast<Uint8 *>(mapped);
  const size_t tile_row_bytes =
      static_cast<size_t>(tile_size) * bytes_per_pixel;
  size_t destination_offset = 0u;
  for (Uint32 layer = 0u; layer < layer_count; ++layer) {
    for (Uint32 row = 0u; row < tile_size; ++row) {
      const size_t source_offset =
          static_cast<size_t>(row) *
              static_cast<size_t>(upload_surface->pitch) +
          static_cast<size_t>(layer) * tile_row_bytes;
      std::memcpy(destination + destination_offset, source + source_offset,
                  tile_row_bytes);
      destination_offset += tile_row_bytes;
    }
  }
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(upload_surface);
    if (log != nullptr) {
      std::fprintf(log, "%s_upload_command=create_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  if (copy_pass == nullptr) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_DestroySurface(upload_surface);
    if (log != nullptr) {
      std::fprintf(log, "%s_copy_pass=create_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  Uint32 transfer_offset = 0u;
  for (Uint32 layer = 0u; layer < layer_count; ++layer) {
    SDL_GPUTextureTransferInfo source_info{};
    source_info.transfer_buffer = transfer;
    source_info.offset = transfer_offset;
    source_info.pixels_per_row = tile_size;
    source_info.rows_per_layer = tile_size;

    SDL_GPUTextureRegion destination_region{};
    destination_region.texture = texture;
    destination_region.layer = layer;
    destination_region.w = tile_size;
    destination_region.h = tile_size;
    destination_region.d = 1u;
    SDL_UploadToGPUTexture(copy_pass, &source_info, &destination_region, false);
    transfer_offset += tile_size * tile_size * bytes_per_pixel;
  }
  SDL_EndGPUCopyPass(copy_pass);

  const bool submitted = SDL_SubmitGPUCommandBuffer(command_buffer);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  SDL_DestroySurface(upload_surface);
  if (!submitted || !SDL_WaitForGPUIdle(device)) {
    if (log != nullptr) {
      std::fprintf(log, "%s_upload=failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }
  return true;
}

} // namespace

bool load_client_block_atlas_texture(SDL_GPUDevice *device, FILE *log,
                                     ClientBlockAtlas &atlas,
                                     const char *filename_suffix,
                                     const char *log_prefix,
                                     ClientBlockAtlasTextureKind kind,
                                     SDL_GPUTexture *&texture) {
  std::string path;
  if (!client_block_atlas_find_first_bundle_file(
          "Assets/Atlases", filename_suffix, log_prefix, log, path)) {
    if (log != nullptr) {
      std::fprintf(log, "%s_path=failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  SDL_Surface *surface = SDL_LoadPNG(path.c_str());
  if (surface == nullptr) {
    if (log != nullptr) {
      std::fprintf(log, "%s=open_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  if (!surface_dimensions_match(surface, atlas, kind)) {
    if (log != nullptr) {
      std::fprintf(log, "%s_width=%d\n", log_prefix, surface->w);
      std::fprintf(log, "%s_height=%d\n", log_prefix, surface->h);
      std::fflush(log);
    }
    SDL_DestroySurface(surface);
    if (log != nullptr) {
      std::fprintf(log, "%s=invalid_dimensions\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  SDL_GPUTextureCreateInfo texture_info{};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
  texture_info.format = kind == ClientBlockAtlasTextureKind::color
                            ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                            : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width = static_cast<Uint32>(atlas.tile_size);
  texture_info.height = static_cast<Uint32>(atlas.tile_size);
  texture_info.layer_count_or_depth =
      static_cast<Uint32>(surface->w / atlas.tile_size);
  texture_info.num_levels = 1u;
  texture = SDL_CreateGPUTexture(device, &texture_info);
  if (texture == nullptr) {
    SDL_DestroySurface(surface);
    if (log != nullptr) {
      std::fprintf(log, "%s=create_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  if (!upload_surface_to_gpu_texture(device, log, surface, log_prefix,
                                     texture)) {
    SDL_ReleaseGPUTexture(device, texture);
    texture = nullptr;
    SDL_DestroySurface(surface);
    return false;
  }
  SDL_DestroySurface(surface);

  if (log != nullptr) {
    std::fprintf(log, "%s=loaded\n", log_prefix);
    std::fflush(log);
  }
  return true;
}

} // namespace octaryn::client::rendering
