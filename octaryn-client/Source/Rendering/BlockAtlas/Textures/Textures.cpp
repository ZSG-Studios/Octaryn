#include "Textures.h"

#include "BundleFile.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <string>
#include <vector>

namespace octaryn::client::rendering {

namespace {

bool surface_dimensions_match(const SDL_Surface *surface,
                              const BlockAtlas &atlas,
                              TextureKind kind) {
  if (surface == nullptr) {
    return false;
  }

  if (kind == TextureKind::animation) {
    return surface->h == atlas.tile_size && surface->w >= atlas.tile_size &&
           surface->w % atlas.tile_size == 0;
  }

  const int32_t expected_width = atlas.layer_count * atlas.tile_size;
  return surface->w == expected_width && surface->h == atlas.tile_size;
}

Uint32 mip_level_count(TextureKind kind, Uint32 tile_size) {
  if (kind == TextureKind::animation) {
    return 1u;
  }

  Uint32 levels = 1u;
  while (tile_size > 1u) {
    tile_size >>= 1u;
    ++levels;
  }
  return levels;
}

void copy_layer_level_zero(const SDL_Surface *surface, Uint32 tile_size,
                           Uint32 layer, std::vector<Uint8> &pixels) {
  pixels.resize(static_cast<size_t>(tile_size) * tile_size * 4u);
  const auto *source = static_cast<const Uint8 *>(surface->pixels);
  const size_t tile_row_bytes = static_cast<size_t>(tile_size) * 4u;
  for (Uint32 row = 0u; row < tile_size; ++row) {
    const size_t source_offset =
        static_cast<size_t>(row) * static_cast<size_t>(surface->pitch) +
        static_cast<size_t>(layer) * tile_row_bytes;
    const size_t destination_offset = static_cast<size_t>(row) * tile_row_bytes;
    std::memcpy(pixels.data() + destination_offset, source + source_offset,
                tile_row_bytes);
  }
}

Uint8 average_channel(const std::vector<Uint8> &source, Uint32 source_size,
                      Uint32 x, Uint32 y, Uint32 channel) {
  uint32_t sum = 0u;
  for (Uint32 oy = 0u; oy < 2u; ++oy) {
    const Uint32 sample_y = std::min(y * 2u + oy, source_size - 1u);
    for (Uint32 ox = 0u; ox < 2u; ++ox) {
      const Uint32 sample_x = std::min(x * 2u + ox, source_size - 1u);
      sum += source[(static_cast<size_t>(sample_y) * source_size + sample_x) *
                        4u +
                    channel];
    }
  }
  return static_cast<Uint8>((sum + 2u) / 4u);
}

void downsample_layer(const std::vector<Uint8> &source, Uint32 source_size,
                      std::vector<Uint8> &destination) {
  const Uint32 destination_size = source_size > 1u ? source_size / 2u : 1u;
  destination.assign(static_cast<size_t>(destination_size) *
                         destination_size * 4u,
                     0u);
  for (Uint32 y = 0u; y < destination_size; ++y) {
    for (Uint32 x = 0u; x < destination_size; ++x) {
      const size_t offset =
          (static_cast<size_t>(y) * destination_size + x) * 4u;
      destination[offset + 0u] = average_channel(source, source_size, x, y, 0u);
      destination[offset + 1u] = average_channel(source, source_size, x, y, 1u);
      destination[offset + 2u] = average_channel(source, source_size, x, y, 2u);
      destination[offset + 3u] = average_channel(source, source_size, x, y, 3u);
    }
  }
}

Uint32 packed_mip_byte_count(Uint32 layer_count, Uint32 mip_levels,
                             Uint32 tile_size) {
  Uint32 total = 0u;
  Uint32 size = tile_size;
  for (Uint32 level = 0u; level < mip_levels; ++level) {
    total += layer_count * size * size * 4u;
    size = size > 1u ? size / 2u : 1u;
  }
  return total;
}

bool upload_surface_to_gpu_texture(SDL_GPUDevice *device, FILE *log,
                                   SDL_Surface *surface, const char *log_prefix,
                                   TextureKind kind, SDL_GPUTexture *texture) {
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
  const Uint32 mip_levels = mip_level_count(kind, tile_size);
  const Uint32 upload_size =
      packed_mip_byte_count(layer_count, mip_levels, tile_size);
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

  Uint8 *destination = static_cast<Uint8 *>(mapped);
  size_t destination_offset = 0u;
  std::vector<Uint8> current;
  std::vector<Uint8> next;
  for (Uint32 layer = 0u; layer < layer_count; ++layer) {
    copy_layer_level_zero(upload_surface, tile_size, layer, current);
    Uint32 level_size = tile_size;
    for (Uint32 level = 0u; level < mip_levels; ++level) {
      const size_t byte_count =
          static_cast<size_t>(level_size) * level_size * bytes_per_pixel;
      std::memcpy(destination + destination_offset, current.data(),
                  byte_count);
      destination_offset += byte_count;
      if (level + 1u < mip_levels) {
        downsample_layer(current, level_size, next);
        current.swap(next);
        level_size = level_size > 1u ? level_size / 2u : 1u;
      }
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
    Uint32 level_size = tile_size;
    for (Uint32 level = 0u; level < mip_levels; ++level) {
      SDL_GPUTextureTransferInfo source_info{};
      source_info.transfer_buffer = transfer;
      source_info.offset = transfer_offset;
      source_info.pixels_per_row = level_size;
      source_info.rows_per_layer = level_size;

      SDL_GPUTextureRegion destination_region{};
      destination_region.texture = texture;
      destination_region.layer = layer;
      destination_region.mip_level = level;
      destination_region.w = level_size;
      destination_region.h = level_size;
      destination_region.d = 1u;
      SDL_UploadToGPUTexture(copy_pass, &source_info, &destination_region,
                             false);
      transfer_offset += level_size * level_size * bytes_per_pixel;
      level_size = level_size > 1u ? level_size / 2u : 1u;
    }
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

bool load_block_atlas_texture(SDL_GPUDevice *device, FILE *log,
                              BlockAtlas &atlas, const char *filename_suffix,
                              const char *log_prefix, TextureKind kind,
                              SDL_GPUTexture *&texture) {
  std::string path;
  if (!find_first_block_atlas_bundle_file(
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
  texture_info.format = kind == TextureKind::color
                            ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                            : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width = static_cast<Uint32>(atlas.tile_size);
  texture_info.height = static_cast<Uint32>(atlas.tile_size);
  texture_info.layer_count_or_depth =
      static_cast<Uint32>(surface->w / atlas.tile_size);
  texture_info.num_levels =
      mip_level_count(kind, static_cast<Uint32>(atlas.tile_size));
  texture = SDL_CreateGPUTexture(device, &texture_info);
  if (texture == nullptr) {
    SDL_DestroySurface(surface);
    if (log != nullptr) {
      std::fprintf(log, "%s=create_failed\n", log_prefix);
      std::fflush(log);
    }
    return false;
  }

  if (!upload_surface_to_gpu_texture(device, log, surface, log_prefix, kind,
                                     texture)) {
    SDL_ReleaseGPUTexture(device, texture);
    texture = nullptr;
    SDL_DestroySurface(surface);
    return false;
  }
  SDL_DestroySurface(surface);

  if (log != nullptr) {
    std::fprintf(log, "%s=loaded mip_levels=%" PRIu32 "\n", log_prefix,
                 texture_info.num_levels);
    std::fflush(log);
  }
  return true;
}

} // namespace octaryn::client::rendering
