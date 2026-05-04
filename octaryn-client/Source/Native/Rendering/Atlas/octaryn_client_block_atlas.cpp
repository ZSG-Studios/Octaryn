#include "octaryn_client_block_atlas.h"

#include "octaryn_client_asset_path.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace octaryn::client::rendering {

namespace block_atlas_json {

struct BlockAtlasLayers {
  int32_t north = 0;
  int32_t south = 0;
  int32_t east = 0;
  int32_t west = 0;
  int32_t up = 0;
  int32_t down = 0;
};

struct BlockCatalogEntry {
  bool placeable = false;
  BlockAtlasLayers atlas;
};

struct BlockCatalogFile {
  std::string schema;
  std::vector<BlockCatalogEntry> blocks;
};

} // namespace block_atlas_json

namespace {

constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};

enum class AtlasTextureKind {
  color,
  material,
  animation,
};

void log_line(FILE *log, const char *message) {
  if (log != nullptr) {
    std::fprintf(log, "%s\n", message);
    std::fflush(log);
  }
}

bool read_text_file(const char *path, const char *failure_label, FILE *log,
                    std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(log, failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool ends_with(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool find_first_bundle_file(const char *relative_directory,
                            const char *filename_suffix,
                            const char *failure_label,
                            FILE *log,
                            std::string &path) {
  char directory_buffer[4096] = {};
  if (!octaryn_client_bundle_path_build(directory_buffer,
                                        sizeof(directory_buffer),
                                        relative_directory)) {
    log_line(log, failure_label);
    return false;
  }

  const std::filesystem::path directory(directory_buffer);
  if (!std::filesystem::exists(directory)) {
    log_line(log, failure_label);
    return false;
  }

  std::vector<std::filesystem::path> matches;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() &&
        ends_with(entry.path().filename().string(), filename_suffix)) {
      matches.push_back(entry.path());
    }
  }

  if (matches.empty()) {
    log_line(log, failure_label);
    return false;
  }

  std::sort(matches.begin(), matches.end());
  path = matches.front().string();
  return true;
}

int32_t manifest_int_value(const std::string &payload, const char *key) {
  const size_t offset = payload.find(key);
  if (offset == std::string::npos) {
    return -1;
  }

  const char *start =
      payload.c_str() + offset + std::char_traits<char>::length(key);
  char *end = nullptr;
  const long value = std::strtol(start, &end, 10);
  if (end == start || value < 0 || value > INT32_MAX) {
    return -1;
  }
  return static_cast<int32_t>(value);
}

bool load_client_block_atlas_manifest(FILE *log, ClientBlockAtlas &atlas) {
  std::string path;
  if (!find_first_bundle_file("Assets/Atlases", "-color.txt",
                              "block_atlas_manifest_path=failed", log, path)) {
    return false;
  }

  std::string payload;
  if (!read_text_file(path.c_str(), "block_atlas_manifest=open_failed", log,
                      payload)) {
    return false;
  }

  atlas.layer_count = manifest_int_value(payload, "layers=");
  atlas.tile_size = manifest_int_value(payload, "tile_size=");
  if (log != nullptr) {
    std::fprintf(log, "block_atlas_layers=%d\n", atlas.layer_count);
    std::fprintf(log, "block_atlas_tile_size=%d\n", atlas.tile_size);
    std::fflush(log);
  }
  if (atlas.layer_count <= 0 || atlas.tile_size <= 0) {
    log_line(log, "block_atlas_manifest=invalid");
    return false;
  }

  log_line(log, "block_atlas_manifest=loaded");
  return true;
}

bool load_block_animation_manifest(FILE *log, ClientBlockAtlas &atlas) {
  std::string path;
  if (!find_first_bundle_file("Assets/Atlases", "-animation.txt",
                              "block_animation_manifest_path=failed", log,
                              path)) {
    return false;
  }

  std::string payload;
  if (!read_text_file(path.c_str(), "block_animation_manifest=open_failed", log,
                      payload)) {
    return false;
  }

  const int32_t animation_tile_size = manifest_int_value(payload, "tile_size=");
  atlas.animation_frames = manifest_int_value(payload, "frames=");
  atlas.animation_count = manifest_int_value(payload, "animations=");
  if (log != nullptr) {
    std::fprintf(log, "block_animation_tile_size=%d\n",
                 animation_tile_size);
    std::fprintf(log, "block_animation_frames=%d\n",
                 atlas.animation_frames);
    std::fprintf(log, "block_animation_count=%d\n",
                 atlas.animation_count);
    std::fflush(log);
  }

  if (animation_tile_size != atlas.tile_size || atlas.animation_frames < 0 ||
      atlas.animation_count < 0) {
    log_line(log, "block_animation_manifest=invalid");
    return false;
  }

  log_line(log, "block_animation_manifest=loaded");
  return true;
}

bool load_block_catalog(FILE *log, ClientBlockAtlas &atlas) {
  std::string path;
  if (!find_first_bundle_file("Data/Blocks", ".blocks.json",
                              "block_catalog_path=failed", log, path)) {
    return false;
  }

  std::string payload;
  if (!read_text_file(path.c_str(), "block_catalog=open_failed", log,
                      payload)) {
    return false;
  }

  block_atlas_json::BlockCatalogFile catalog{};
  const auto error = glz::read<kJsonReadOptions>(catalog, payload);
  if (error) {
    log_line(log, "block_catalog=parse_failed");
    return false;
  }

  if (!ends_with(catalog.schema, ".blocks.v1") || catalog.blocks.empty()) {
    log_line(log, "block_catalog=invalid");
    return false;
  }

  atlas.block_top_layers.clear();
  atlas.placeable_blocks.clear();
  atlas.block_top_layers.reserve(catalog.blocks.size());
  atlas.placeable_blocks.reserve(catalog.blocks.size());
  for (size_t index = 0; index < catalog.blocks.size(); ++index) {
    const auto &block = catalog.blocks[index];
    if (block.atlas.up < 0 || block.atlas.up >= atlas.layer_count) {
      log_line(log, "block_catalog=invalid_atlas_layer");
      return false;
    }
    atlas.block_top_layers.push_back(block.atlas.up);
    if (block.placeable && index <= UINT16_MAX) {
      atlas.placeable_blocks.push_back(static_cast<uint16_t>(index));
    }
  }

  if (log != nullptr) {
    std::fprintf(log, "block_catalog_entries=%zu\n",
                 atlas.block_top_layers.size());
    std::fprintf(log, "block_atlas_placeable_blocks=%zu\n",
                 atlas.placeable_blocks.size());
    std::fflush(log);
  }
  log_line(log, "block_catalog=loaded");
  return true;
}

bool surface_dimensions_match(const SDL_Surface *surface,
                              const ClientBlockAtlas &atlas,
                              AtlasTextureKind kind) {
  if (surface == nullptr) {
    return false;
  }

  if (kind == AtlasTextureKind::animation) {
    return surface->h == atlas.tile_size && surface->w >= atlas.tile_size &&
           surface->w % atlas.tile_size == 0;
  }

  const int32_t expected_width = atlas.layer_count * atlas.tile_size;
  return surface->w == expected_width && surface->h == atlas.tile_size;
}

bool upload_surface_to_gpu_texture(SDL_GPUDevice *device, FILE *log,
                                   SDL_Surface *surface,
                                   const char *log_prefix,
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
  const size_t tile_row_bytes = static_cast<size_t>(tile_size) * bytes_per_pixel;
  size_t destination_offset = 0u;
  for (Uint32 layer = 0u; layer < layer_count; ++layer) {
    for (Uint32 row = 0u; row < tile_size; ++row) {
      const size_t source_offset =
          static_cast<size_t>(row) * static_cast<size_t>(upload_surface->pitch) +
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

bool load_client_block_atlas_texture(SDL_GPUDevice *device, FILE *log,
                                 ClientBlockAtlas &atlas,
                                 const char *filename_suffix,
                                 const char *log_prefix,
                                 AtlasTextureKind kind,
                                 SDL_GPUTexture *&texture) {
  std::string path;
  if (!find_first_bundle_file("Assets/Atlases", filename_suffix, log_prefix,
                              log, path)) {
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
  texture_info.format = kind == AtlasTextureKind::color
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

} // namespace

bool load_client_block_atlas(SDL_GPUDevice *device, FILE *log,
                         ClientBlockAtlas &atlas) {
  atlas.device = device;
  const bool loaded =
      load_client_block_atlas_manifest(log, atlas) &&
      load_block_animation_manifest(log, atlas) &&
      load_block_catalog(log, atlas) &&
      load_client_block_atlas_texture(device, log, atlas,
                                  "-color.png",
                                  "block_atlas_texture",
                                  AtlasTextureKind::color,
                                  atlas.color_texture) &&
      load_client_block_atlas_texture(device, log, atlas,
                                  "-normal.png",
                                  "block_atlas_normal_texture",
                                  AtlasTextureKind::material,
                                  atlas.normal_texture) &&
      load_client_block_atlas_texture(device, log, atlas,
                                  "-specular.png",
                                  "block_atlas_specular_texture",
                                  AtlasTextureKind::material,
                                  atlas.specular_texture) &&
      load_client_block_atlas_texture(device, log, atlas,
                                  "-animation.png",
                                  "block_atlas_animation_texture",
                                  AtlasTextureKind::animation,
                                  atlas.animation_texture);
  if (!loaded) {
    destroy_client_block_atlas(atlas);
  }
  return loaded;
}

void destroy_client_block_atlas(ClientBlockAtlas &atlas) {
  if (atlas.animation_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.animation_texture);
    atlas.animation_texture = nullptr;
  }
  if (atlas.specular_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.specular_texture);
    atlas.specular_texture = nullptr;
  }
  if (atlas.normal_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.normal_texture);
    atlas.normal_texture = nullptr;
  }
  if (atlas.color_texture != nullptr) {
    SDL_ReleaseGPUTexture(atlas.device, atlas.color_texture);
    atlas.color_texture = nullptr;
  }
  atlas.device = nullptr;
}

int32_t client_block_atlas_top_layer_for_block(const ClientBlockAtlas &atlas,
                                           uint16_t block) {
  return block < atlas.block_top_layers.size() ? atlas.block_top_layers[block]
                                               : -1;
}

uint16_t client_block_atlas_default_placeable_block(const ClientBlockAtlas &atlas,
                                                uint16_t fallback) {
  if (std::find(atlas.placeable_blocks.begin(), atlas.placeable_blocks.end(),
                fallback) != atlas.placeable_blocks.end()) {
    return fallback;
  }
  return atlas.placeable_blocks.empty() ? fallback : atlas.placeable_blocks[0];
}

uint16_t client_block_atlas_scroll_placeable_block(const ClientBlockAtlas &atlas,
                                               uint16_t current, int delta) {
  if (delta == 0 || atlas.placeable_blocks.empty()) {
    return client_block_atlas_default_placeable_block(atlas, current);
  }

  const auto iterator =
      std::find(atlas.placeable_blocks.begin(), atlas.placeable_blocks.end(),
                current);
  const ptrdiff_t start =
      iterator == atlas.placeable_blocks.end()
          ? 0
          : std::distance(atlas.placeable_blocks.begin(), iterator);
  const ptrdiff_t count =
      static_cast<ptrdiff_t>(atlas.placeable_blocks.size());
  const ptrdiff_t wrapped = (start + delta) % count;
  const ptrdiff_t index = wrapped < 0 ? wrapped + count : wrapped;
  return atlas.placeable_blocks[static_cast<size_t>(index)];
}

} // namespace octaryn::client::rendering
