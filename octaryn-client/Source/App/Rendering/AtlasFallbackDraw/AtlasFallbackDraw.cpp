#include "AtlasFallbackDraw.h"

#include "Environment.h"
#include "Log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace octaryn_client_app {
namespace {

using octaryn::client::rendering::block_atlas_top_layer_for_block;

constexpr int kBlockDrawSize = 48;
constexpr int kWorldBlockDrawSize = 8;
constexpr float kRenderDistanceBlocks = 42.0f;
constexpr int kMaterialAtlasProbeLayer = 16;
constexpr int kMaterialAtlasProbeY = 8;
constexpr int kMaterialAtlasProbeNormalX = 8;
constexpr int kMaterialAtlasProbeSpecularX = 40;
constexpr int kMaterialAtlasProbeSize = 24;
constexpr const char *kPixelValidationFlag =
    "OCTARYN_CLIENT_APP_VALIDATE_PIXELS";

int block_draw_size_for(size_t block_count) {
  return block_count > 1u ? kWorldBlockDrawSize : kBlockDrawSize;
}

bool blit_gpu_texture(SDL_GPUCommandBuffer *command_buffer,
                      SDL_GPUTexture *source_texture,
                      SDL_GPUTexture *target_texture, uint32_t source_layer,
                      int source_x, int source_y, int source_size, int target_x,
                      int target_y, int target_size, uint32_t target_width,
                      uint32_t target_height) {
  if (target_x + target_size <= 0 || target_y + target_size <= 0 ||
      target_x >= static_cast<int>(target_width) ||
      target_y >= static_cast<int>(target_height)) {
    return true;
  }

  SDL_GPUBlitInfo blit{};
  blit.source.texture = source_texture;
  blit.source.layer_or_depth_plane = source_layer;
  blit.source.x = static_cast<Uint32>(source_x);
  blit.source.y = static_cast<Uint32>(source_y);
  blit.source.w = static_cast<Uint32>(source_size);
  blit.source.h = static_cast<Uint32>(source_size);
  blit.destination.texture = target_texture;
  blit.destination.x = static_cast<Uint32>(std::max(target_x, 0));
  blit.destination.y = static_cast<Uint32>(std::max(target_y, 0));
  blit.destination.w = static_cast<Uint32>(target_size);
  blit.destination.h = static_cast<Uint32>(target_size);
  blit.load_op = SDL_GPU_LOADOP_LOAD;
  blit.filter = SDL_GPU_FILTER_NEAREST;
  SDL_BlitGPUTexture(command_buffer, &blit);
  return true;
}

void block_screen_position(const presentation_block &block,
                           const camera &camera,
                           int block_draw_size, uint32_t target_width,
                           uint32_t target_height, int &screen_x,
                           int &screen_y) {
  const float relative_x = static_cast<float>(block.x) - camera.position[0];
  const float relative_y = static_cast<float>(block.y) - camera.position[1];
  const float relative_z = static_cast<float>(block.z) - camera.position[2];
  const float yaw_sine = std::sin(-camera.yaw_radians);
  const float yaw_cosine = std::cos(-camera.yaw_radians);
  const float view_x = relative_x * yaw_cosine - relative_z * yaw_sine;
  const float view_z = relative_x * yaw_sine + relative_z * yaw_cosine;
  const float pitch_lift = std::sin(camera.pitch_radians) * view_z *
                           static_cast<float>(block_draw_size) * 0.75f;
  const float projected_x =
      static_cast<float>(target_width) * 0.5f +
      view_x * static_cast<float>(block_draw_size) +
      view_z * static_cast<float>(block_draw_size) * 0.5f -
      static_cast<float>(block_draw_size) * 0.5f;
  const float projected_y =
      static_cast<float>(target_height) * 0.5f -
      relative_y * static_cast<float>(block_draw_size) -
      view_z * static_cast<float>(block_draw_size) / 3.0f + pitch_lift -
      static_cast<float>(block_draw_size) * 0.5f;
  screen_x = static_cast<int>(projected_x);
  screen_y = static_cast<int>(projected_y);
}

} // namespace

bool draw_atlas_fallback_blocks(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::BlockAtlas &atlas,
    const std::vector<presentation_block> &blocks,
    const camera &camera, int &drawn_tiles) {
  const int block_draw_size = block_draw_size_for(blocks.size());
  drawn_tiles = 0;
  for (const presentation_block &block : blocks) {
    const float distance_x = static_cast<float>(block.x) - camera.position[0];
    const float distance_z = static_cast<float>(block.z) - camera.position[2];
    if (distance_x * distance_x + distance_z * distance_z >
        kRenderDistanceBlocks * kRenderDistanceBlocks) {
      continue;
    }

    const int32_t layer =
        block_atlas_top_layer_for_block(atlas, block.block);
    if (layer < 0) {
      continue;
    }

    int screen_x = 0;
    int screen_y = 0;
    block_screen_position(block, camera, block_draw_size, target_width,
                          target_height, screen_x, screen_y);
    if (!blit_gpu_texture(command_buffer, atlas.color_texture, target_texture,
                          static_cast<uint32_t>(layer), 0, 0, atlas.tile_size,
                          screen_x, screen_y, block_draw_size, target_width,
                          target_height)) {
      return false;
    }
    ++drawn_tiles;
  }

  if (drawn_tiles != 0 && g_log != nullptr) {
    std::fprintf(g_log, "atlas_tiles_drawn=%d\n", drawn_tiles);
    std::fprintf(g_log,
                 "visible_render_distance blocks=%.1f drawn_surface_blocks=%d "
                 "source=atlas_blit_fallback\n",
                 kRenderDistanceBlocks, drawn_tiles);
    std::fflush(g_log);
  }
  return true;
}

bool draw_material_atlas_probe(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    uint32_t target_width, uint32_t target_height,
    const octaryn::client::rendering::BlockAtlas &atlas) {
  if (!read_enabled_flag(kPixelValidationFlag)) {
    return true;
  }
  if (atlas.normal_texture == nullptr || atlas.specular_texture == nullptr ||
      kMaterialAtlasProbeLayer >= atlas.layer_count) {
    log_line("material_atlas_probe=invalid");
    return false;
  }

  if (!blit_gpu_texture(command_buffer, atlas.normal_texture, target_texture,
                        static_cast<uint32_t>(kMaterialAtlasProbeLayer), 0, 0,
                        atlas.tile_size, kMaterialAtlasProbeNormalX,
                        kMaterialAtlasProbeY, kMaterialAtlasProbeSize,
                        target_width, target_height) ||
      !blit_gpu_texture(command_buffer, atlas.specular_texture, target_texture,
                        static_cast<uint32_t>(kMaterialAtlasProbeLayer), 0, 0,
                        atlas.tile_size, kMaterialAtlasProbeSpecularX,
                        kMaterialAtlasProbeY, kMaterialAtlasProbeSize,
                        target_width, target_height)) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log, "material_atlas_tiles_drawn=2\n");
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
