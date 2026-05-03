#include "octaryn_client_asset_path.h"
#include "octaryn_client_basegame_atlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_host_exports.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_native_crash_diagnostics.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace octaryn_client_app {

struct world_block_record {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct world_block_file {
  int32_t version = 0;
  std::vector<world_block_record> blocks;
};

} // namespace octaryn_client_app

namespace {

using octaryn::client::rendering::basegame_atlas_top_layer_for_block;
using octaryn::client::rendering::BasegameAtlas;
using octaryn::client::rendering::destroy_basegame_atlas;
using octaryn::client::rendering::load_basegame_atlas;
using octaryn_client_app::world_block_file;
using octaryn_client_app::world_block_record;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr Uint8 kClearRed = 18;
constexpr Uint8 kClearGreen = 43;
constexpr Uint8 kClearBlue = 49;
constexpr Uint8 kClearAlpha = 255;
constexpr int kBlockDrawSize = 48;
constexpr int kWorldBlockDrawSize = 8;
constexpr int kMaterialAtlasProbeLayer = 16;
constexpr int kMaterialAtlasProbeY = 8;
constexpr int kMaterialAtlasProbeNormalX = 8;
constexpr int kMaterialAtlasProbeSpecularX = 40;
constexpr int kMaterialAtlasProbeSize = 24;
constexpr int kWorldSnapshotMinX = 0;
constexpr int kWorldSnapshotMaxXExclusive = 32;
constexpr int kWorldSnapshotMinZ = 0;
constexpr int kWorldSnapshotMaxZExclusive = 32;
constexpr int kMaxPresentationUpdatesPerFrame = 256;
constexpr float kFlySpeedBlocksPerSecond = 9.6f;
constexpr float kFlyFastSpeedBlocksPerSecond = 24.0f;
constexpr float kMouseSensitivityDegrees = 0.1f;
constexpr const char *kInputProbeFlag = "OCTARYN_CLIENT_APP_INPUT_PROBE";
constexpr const char *kPixelValidationFlag =
    "OCTARYN_CLIENT_APP_VALIDATE_PIXELS";
constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};
constexpr uint32_t kInputJumpFlag = 1u << 0u;
constexpr uint32_t kInputSprintFlag = 1u << 1u;
constexpr uint32_t kInputFlyModeFlag = 1u << 2u;
constexpr uint32_t kInputPrimaryFlag = 1u << 3u;
constexpr uint32_t kInputSecondaryFlag = 1u << 4u;

FILE *g_log;

struct presentation_block {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct client_input_debug_state {
  uint32_t flags = 0u;
  uint32_t controller = 0u;
  float move_x = 0.0f;
  float move_y = 0.0f;
  float move_z = 0.0f;
  float look_pitch = 0.0f;
  float look_yaw = 0.0f;
  float speed = kFlySpeedBlocksPerSecond;
  int relative_mouse = 0;
  bool active = false;
};

struct pointer_motion_debug_state {
  float xrel = 0.0f;
  float yrel = 0.0f;
};

bool renderer_output_size(SDL_Renderer *renderer, int *width, int *height);

void log_line(const char *message) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s\n", message);
    std::fflush(g_log);
  }
}

void log_result(const char *name, int result) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s=%d\n", name, result);
    std::fflush(g_log);
  }
}

uint32_t read_exit_after_frames() {
  const char *value = std::getenv("OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES");
  if (value == nullptr || value[0] == '\0') {
    return 0;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value) {
    return 0;
  }

  return parsed > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(parsed);
}

bool read_enabled_flag(const char *name) {
  const char *value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

bool key_down(const bool *keys, SDL_Scancode scancode) {
  return keys != nullptr && keys[scancode];
}

client_input_debug_state
read_client_input(SDL_Window *window,
                  const pointer_motion_debug_state &pointer_motion) {
  client_input_debug_state input{};
  const bool *keys = SDL_GetKeyboardState(nullptr);
  input.move_x = (key_down(keys, SDL_SCANCODE_D) ? 1.0f : 0.0f) -
                 (key_down(keys, SDL_SCANCODE_A) ? 1.0f : 0.0f);
  input.move_y =
      (key_down(keys, SDL_SCANCODE_SPACE) || key_down(keys, SDL_SCANCODE_E)
           ? 1.0f
           : 0.0f) -
      (key_down(keys, SDL_SCANCODE_Q) || key_down(keys, SDL_SCANCODE_LSHIFT) ||
               key_down(keys, SDL_SCANCODE_RSHIFT)
           ? 1.0f
           : 0.0f);
  input.move_z = (key_down(keys, SDL_SCANCODE_W) ? 1.0f : 0.0f) -
                 (key_down(keys, SDL_SCANCODE_S) ? 1.0f : 0.0f);

  if (key_down(keys, SDL_SCANCODE_SPACE)) {
    input.flags |= kInputJumpFlag;
  }
  if (key_down(keys, SDL_SCANCODE_LCTRL) ||
      key_down(keys, SDL_SCANCODE_RCTRL)) {
    input.flags |= kInputSprintFlag;
    input.speed = kFlyFastSpeedBlocksPerSecond;
  }
  input.flags |= kInputFlyModeFlag;

  const SDL_MouseButtonFlags mouse_buttons =
      SDL_GetMouseState(nullptr, nullptr);
  if ((mouse_buttons & SDL_BUTTON_LMASK) != 0u) {
    input.flags |= kInputPrimaryFlag;
  }
  if ((mouse_buttons & SDL_BUTTON_RMASK) != 0u) {
    input.flags |= kInputSecondaryFlag;
  }
  input.relative_mouse = SDL_GetWindowRelativeMouseMode(window) ? 1 : 0;
  if (input.relative_mouse != 0) {
    input.look_pitch = -pointer_motion.yrel * kMouseSensitivityDegrees;
    input.look_yaw = pointer_motion.xrel * kMouseSensitivityDegrees;
  }
  input.active =
      input.move_x != 0.0f || input.move_y != 0.0f || input.move_z != 0.0f ||
      input.look_pitch != 0.0f || input.look_yaw != 0.0f ||
      (input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u ||
      input.relative_mouse != 0;
  return input;
}

void apply_input_probe(client_input_debug_state &input, uint64_t frame_index) {
  if (!read_enabled_flag(kInputProbeFlag) || frame_index != 1u) {
    return;
  }

  input.controller = 1u;
  input.move_x = 1.0f;
  input.move_y = 1.0f;
  input.move_z = 1.0f;
  input.look_pitch = -6.0f;
  input.look_yaw = 12.0f;
  input.speed = kFlyFastSpeedBlocksPerSecond;
  input.relative_mouse = 1;
  input.flags |= kInputJumpFlag | kInputSprintFlag | kInputFlyModeFlag |
                 kInputPrimaryFlag | kInputSecondaryFlag;
  input.active = true;
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command *command) {
  if (g_log != nullptr && command != nullptr) {
    std::fprintf(g_log,
                 "live_client_command_enqueue kind=%" PRIu32 " request=%" PRIu64
                 " target=%" PRIu64 " block=(%" PRId32 ",%" PRId32 ",%" PRId32
                 ",%" PRId32 ")\n",
                 command->kind, command->request_id, command->target_id,
                 command->a, command->b, command->c, command->d);
    std::fflush(g_log);
  }

  return 1;
}

octaryn_host_frame_snapshot create_frame(uint64_t frame_index,
                                         double delta_seconds) {
  octaryn_host_frame_snapshot frame{};
  frame.version = 1u;
  frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame.input.version = 1u;
  frame.input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
  frame.timing.version = 1u;
  frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame.timing.frame_index = frame_index;
  frame.timing.delta_seconds = delta_seconds;
  return frame;
}

void apply_input_to_frame(octaryn_host_frame_snapshot &frame,
                          const client_input_debug_state &input,
                          const octaryn_client_camera &camera) {
  frame.input.flags = input.flags;
  frame.input.controller = input.controller;
  frame.input.move_x = input.move_x;
  frame.input.move_y = input.move_y;
  frame.input.move_z = input.move_z;
  frame.input.camera_x = camera.position[0];
  frame.input.camera_y = camera.position[1];
  frame.input.camera_z = camera.position[2];
  frame.input.camera_pitch = camera.pitch_radians;
  frame.input.camera_yaw = camera.yaw_radians;
  frame.input.relative_mouse = input.relative_mouse;
}

void log_client_tick_input_frame(const octaryn_host_frame_snapshot &frame) {
  if (g_log == nullptr) {
    return;
  }

  std::fprintf(g_log,
               "live_client_tick_input frame=%" PRIu64
               " dt=%.6f flags=%" PRIu32 " controller=%" PRIu32
               " move=(%.3f,%.3f,%.3f)"
               " camera=(%.3f,%.3f,%.3f,%.6f,%.6f)"
               " relative_mouse=%" PRId32 "\n",
               frame.timing.frame_index, frame.timing.delta_seconds,
               frame.input.flags, frame.input.controller, frame.input.move_x,
               frame.input.move_y, frame.input.move_z, frame.input.camera_x,
               frame.input.camera_y, frame.input.camera_z,
               frame.input.camera_pitch, frame.input.camera_yaw,
               frame.input.relative_mouse);
  std::fflush(g_log);
}

bool update_client_camera(SDL_Renderer *renderer, octaryn_client_camera &camera,
                          const client_input_debug_state &input,
                          double delta_seconds) {
  int render_width = 0;
  int render_height = 0;
  if (!renderer_output_size(renderer, &render_width, &render_height)) {
    return false;
  }

  octaryn_client_camera_resize(&camera, render_width, render_height);
  if (input.look_pitch != 0.0f || input.look_yaw != 0.0f) {
    octaryn_client_camera_rotate_degrees(&camera, input.look_pitch,
                                         input.look_yaw);
  }

  const float dx =
      input.move_x * input.speed * static_cast<float>(delta_seconds);
  const float dy =
      input.move_y * input.speed * static_cast<float>(delta_seconds);
  const float dz =
      input.move_z * input.speed * static_cast<float>(delta_seconds);
  if (dx != 0.0f || dy != 0.0f || dz != 0.0f) {
    octaryn_client_camera_move(&camera, dx, dy, dz);
  }
  octaryn_client_camera_update(&camera);
  return true;
}

double frame_delta_seconds(uint64_t previous_ticks, uint64_t current_ticks) {
  if (previous_ticks == 0u || current_ticks <= previous_ticks) {
    return kDefaultDeltaSeconds;
  }

  return static_cast<double>(current_ticks - previous_ticks) / 1000000000.0;
}

uint64_t pack_signed_pair(int32_t a, int32_t b) {
  return static_cast<uint32_t>(a) |
         (static_cast<uint64_t>(static_cast<uint32_t>(b)) << 32u);
}

uint64_t pack_block(int32_t z, uint16_t block) {
  return static_cast<uint32_t>(z) | (static_cast<uint64_t>(block) << 32u);
}

int32_t unpack_low(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int32_t unpack_high(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value >> 32u));
}

int apply_probe_snapshot() {
  octaryn_replication_change changes[1]{};
  changes[0].version = 1u;
  changes[0].size = OCTARYN_REPLICATION_CHANGE_SIZE;
  changes[0].change_kind = 1u;
  changes[0].replication_id = 1u;
  changes[0].payload0 = pack_signed_pair(0, 0);
  changes[0].payload1 = pack_block(0, 7u);

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = 1u;
  snapshot.tick_id = 1u;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(changes));

  return octaryn_client_apply_server_snapshot(&snapshot);
}

bool read_text_file(const char *path, const char *failure_label,
                    std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool load_basegame_module_descriptor() {
  char path[4096] = {};
  if (!octaryn_client_bundle_path_build(
          path, sizeof(path), "Data/Module/octaryn.basegame.module.json")) {
    log_line("basegame_module_descriptor_path=failed");
    return false;
  }

  std::string payload;
  if (!read_text_file(path, "basegame_module_descriptor=open_failed",
                      payload)) {
    return false;
  }

  if (payload.find("octaryn.basegame") == std::string::npos) {
    log_line("basegame_module_descriptor=invalid");
    return false;
  }

  log_line("basegame_module_descriptor=loaded");
  return true;
}

uint64_t pack_column_key(int32_t x, int32_t z) {
  return static_cast<uint32_t>(x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(z)) << 32u);
}

bool is_spawn_column_block(const world_block_record &block) {
  return block.block != 0u && block.x >= kWorldSnapshotMinX &&
         block.x < kWorldSnapshotMaxXExclusive &&
         block.z >= kWorldSnapshotMinZ && block.z < kWorldSnapshotMaxZExclusive;
}

bool load_world_snapshot_blocks(std::vector<presentation_block> &blocks) {
  const char *path = std::getenv("OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH");
  if (path == nullptr || path[0] == '\0') {
    log_line("live_chunk_streaming active=0 source=none surface_blocks=0 "
             "reason=no_runtime_chunk_streaming");
    return true;
  }

  std::string payload;
  if (!read_text_file(path, "world_blocks_file=open_failed", payload)) {
    return false;
  }

  world_block_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, payload);
  if (error) {
    log_line("world_blocks_file=parse_failed");
    return false;
  }

  if (file.version != 1) {
    log_line("world_blocks_file=unsupported_version");
    return false;
  }

  std::unordered_map<uint64_t, presentation_block> top_blocks;
  for (const world_block_record &record : file.blocks) {
    if (!is_spawn_column_block(record)) {
      continue;
    }

    const uint64_t key = pack_column_key(record.x, record.z);
    const auto iterator = top_blocks.find(key);
    if (iterator == top_blocks.end() || record.y > iterator->second.y) {
      top_blocks[key] =
          presentation_block{record.x, record.y, record.z, record.block};
    }
  }

  blocks.clear();
  blocks.reserve(top_blocks.size());
  for (const auto &entry : top_blocks) {
    blocks.push_back(entry.second);
  }

  std::sort(
      blocks.begin(), blocks.end(),
      [](const presentation_block &left, const presentation_block &right) {
        if (left.x != right.x) {
          return left.x < right.x;
        }

        return left.z < right.z;
      });

  if (g_log != nullptr) {
    std::fprintf(g_log, "world_blocks_loaded=%zu\n", file.blocks.size());
    std::fprintf(g_log, "world_surface_blocks_applied=%zu\n", blocks.size());
    std::fprintf(g_log,
                 "live_chunk_streaming active=0 source=world_blocks_path "
                 "loaded=%zu surface_blocks=%zu reason=static_snapshot\n",
                 file.blocks.size(), blocks.size());
    std::fflush(g_log);
  }
  return !blocks.empty();
}

int apply_snapshot_blocks(const std::vector<presentation_block> &blocks,
                          uint64_t tick_id) {
  std::vector<octaryn_replication_change> changes(blocks.size());
  for (size_t index = 0; index < blocks.size(); ++index) {
    const presentation_block &block = blocks[index];
    changes[index].version = 1u;
    changes[index].size = OCTARYN_REPLICATION_CHANGE_SIZE;
    changes[index].change_kind = 1u;
    changes[index].replication_id = static_cast<uint64_t>(index + 1u);
    changes[index].payload0 = pack_signed_pair(block.x, block.y);
    changes[index].payload1 = pack_block(block.z, block.block);
  }

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = static_cast<uint32_t>(changes.size());
  snapshot.tick_id = tick_id;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(changes.data()));

  return octaryn_client_apply_server_snapshot(&snapshot);
}

void apply_presentation_update(std::vector<presentation_block> &blocks,
                               const octaryn_replication_change &change) {
  if (change.version != 1u || change.size != OCTARYN_REPLICATION_CHANGE_SIZE ||
      change.change_kind != 1u) {
    return;
  }

  presentation_block update{};
  update.x = unpack_low(change.payload0);
  update.y = unpack_high(change.payload0);
  update.z = unpack_low(change.payload1);
  update.block = static_cast<uint16_t>(change.payload1 >> 32u);

  for (auto iterator = blocks.begin(); iterator != blocks.end(); ++iterator) {
    if (iterator->x == update.x && iterator->y == update.y &&
        iterator->z == update.z) {
      if (update.block == 0u) {
        blocks.erase(iterator);
      } else {
        *iterator = update;
      }
      return;
    }
  }

  if (update.block != 0u) {
    blocks.push_back(update);
  }
}

bool drain_presentation_updates(std::vector<presentation_block> &blocks,
                                uint32_t &written) {
  octaryn_replication_change changes[kMaxPresentationUpdatesPerFrame]{};
  written = 0u;
  const int result = octaryn_client_drain_presentation_updates(
      changes, kMaxPresentationUpdatesPerFrame, &written);
  if (result != 0) {
    log_result("drain_presentation_updates", result);
    return false;
  }

  for (uint32_t index = 0u; index < written; ++index) {
    apply_presentation_update(blocks, changes[index]);
  }

  if (written != 0u && g_log != nullptr) {
    std::fprintf(g_log, "presentation_updates_drained=%" PRIu32 "\n", written);
    std::fflush(g_log);
  }
  return true;
}

void log_live_client_frame(uint64_t frame_index,
                           const client_input_debug_state &input,
                           const octaryn_client_camera &camera,
                           uint32_t drained_updates,
                           const std::vector<presentation_block> &blocks) {
  if (g_log == nullptr) {
    return;
  }

  if (frame_index == 1u || input.active || drained_updates != 0u ||
      frame_index % 60u == 0u) {
    std::fprintf(g_log,
                 "live_input_frame frame=%" PRIu64
                 " active=%d move=(%.3f,%.3f,%.3f) flags=%" PRIu32
                 " controller=%" PRIu32 " relative_mouse=%d\n",
                 frame_index, input.active ? 1 : 0, input.move_x, input.move_y,
                 input.move_z, input.flags, input.controller,
                 input.relative_mouse);
    std::fprintf(g_log,
                 "live_camera_frame frame=%" PRIu64
                 " active=%d mode=live_runtime x=%.3f y=%.3f z=%.3f"
                 " pitch=%.6f yaw=%.6f look=(%.3f,%.3f)\n",
                 frame_index, input.active ? 1 : 0, camera.position[0],
                 camera.position[1], camera.position[2], camera.pitch_radians,
                 camera.yaw_radians, input.look_pitch, input.look_yaw);
    std::fprintf(g_log,
                 "live_movement_frame frame=%" PRIu64
                 " active=%d speed=%.3f move=(%.3f,%.3f,%.3f)"
                 " sprint=%d fly=1\n",
                 frame_index, input.active ? 1 : 0, input.speed, input.move_x,
                 input.move_y, input.move_z,
                 (input.flags & kInputSprintFlag) != 0u ? 1 : 0);
    std::fprintf(g_log,
                 "live_interaction_frame frame=%" PRIu64
                 " primary=%d secondary=%d command_enqueue_hook=active\n",
                 frame_index, (input.flags & kInputPrimaryFlag) != 0u ? 1 : 0,
                 (input.flags & kInputSecondaryFlag) != 0u ? 1 : 0);
    std::fprintf(g_log,
                 "live_presentation_frame frame=%" PRIu64
                 " blocks=%zu drained_updates=%" PRIu32 "\n",
                 frame_index, blocks.size(), drained_updates);
    std::fflush(g_log);
  }
}

bool renderer_output_size(SDL_Renderer *renderer, int *width, int *height) {
  if (!SDL_GetRenderOutputSize(renderer, width, height)) {
    log_line("render_output_size=failed");
    return false;
  }

  if (*width <= 0 || *height <= 0) {
    log_line("render_output_size=invalid");
    return false;
  }

  return true;
}

int block_draw_size_for(size_t block_count) {
  return block_count > 1u ? kWorldBlockDrawSize : kBlockDrawSize;
}

bool draw_blocks(SDL_Renderer *renderer, const BasegameAtlas &atlas,
                 const std::vector<presentation_block> &blocks) {
  int render_width = 0;
  int render_height = 0;
  if (!renderer_output_size(renderer, &render_width, &render_height)) {
    return false;
  }

  const int block_draw_size = block_draw_size_for(blocks.size());
  int drawn_tiles = 0;
  for (const presentation_block &block : blocks) {
    const int32_t layer =
        basegame_atlas_top_layer_for_block(atlas, block.block);
    if (layer < 0) {
      continue;
    }

    SDL_FRect source{
        static_cast<float>(layer * atlas.tile_size),
        0.0f,
        static_cast<float>(atlas.tile_size),
        static_cast<float>(atlas.tile_size),
    };
    const float screen_x =
        static_cast<float>(render_width / 2 + block.x * block_draw_size +
                           block.z * block_draw_size / 2 - block_draw_size / 2);
    const float screen_y =
        static_cast<float>(render_height / 2 - block.y * block_draw_size -
                           block.z * block_draw_size / 3 - block_draw_size / 2);
    SDL_FRect rect{screen_x, screen_y, static_cast<float>(block_draw_size),
                   static_cast<float>(block_draw_size)};
    if (!SDL_RenderTexture(renderer, atlas.color_texture, &source, &rect)) {
      log_line("atlas_tile_draw=failed");
      return false;
    }
    ++drawn_tiles;
  }

  if (drawn_tiles != 0 && g_log != nullptr) {
    std::fprintf(g_log, "atlas_tiles_drawn=%d\n", drawn_tiles);
    std::fflush(g_log);
  }
  return true;
}

bool render_texture_without_blend(SDL_Renderer *renderer, SDL_Texture *texture,
                                  const SDL_FRect &source,
                                  const SDL_FRect &rect) {
  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  if (!SDL_GetTextureBlendMode(texture, &previous_blend_mode)) {
    log_line("material_atlas_blend_mode=get_failed");
    return false;
  }

  if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE)) {
    log_line("material_atlas_blend_mode=set_failed");
    return false;
  }

  const bool rendered = SDL_RenderTexture(renderer, texture, &source, &rect);
  const bool restored = SDL_SetTextureBlendMode(texture, previous_blend_mode);
  if (!rendered) {
    log_line("material_atlas_tile_draw=failed");
    return false;
  }
  if (!restored) {
    log_line("material_atlas_blend_mode=restore_failed");
    return false;
  }
  return true;
}

bool draw_material_atlas_probe(SDL_Renderer *renderer,
                               const BasegameAtlas &atlas) {
  if (!read_enabled_flag(kPixelValidationFlag)) {
    return true;
  }
  if (atlas.normal_texture == nullptr || atlas.specular_texture == nullptr ||
      kMaterialAtlasProbeLayer >= atlas.layer_count) {
    log_line("material_atlas_probe=invalid");
    return false;
  }

  const SDL_FRect source{
      static_cast<float>(kMaterialAtlasProbeLayer * atlas.tile_size),
      0.0f,
      static_cast<float>(atlas.tile_size),
      static_cast<float>(atlas.tile_size),
  };
  const SDL_FRect normal_rect{
      static_cast<float>(kMaterialAtlasProbeNormalX),
      static_cast<float>(kMaterialAtlasProbeY),
      static_cast<float>(kMaterialAtlasProbeSize),
      static_cast<float>(kMaterialAtlasProbeSize),
  };
  const SDL_FRect specular_rect{
      static_cast<float>(kMaterialAtlasProbeSpecularX),
      static_cast<float>(kMaterialAtlasProbeY),
      static_cast<float>(kMaterialAtlasProbeSize),
      static_cast<float>(kMaterialAtlasProbeSize),
  };

  if (!render_texture_without_blend(renderer, atlas.normal_texture, source,
                                    normal_rect) ||
      !render_texture_without_blend(renderer, atlas.specular_texture, source,
                                    specular_rect)) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log, "material_atlas_tiles_drawn=2\n");
    std::fflush(g_log);
  }
  return true;
}

bool color_matches(Uint8 red, Uint8 green, Uint8 blue, Uint8 expected_red,
                   Uint8 expected_green, Uint8 expected_blue) {
  return red == expected_red && green == expected_green &&
         blue == expected_blue;
}

bool inside_rect(int x, int y, int rect_x, int rect_y, int rect_size) {
  return x >= rect_x && x < rect_x + rect_size && y >= rect_y &&
         y < rect_y + rect_size;
}

bool validate_render_pixels(SDL_Renderer *renderer) {
  SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
  if (surface == nullptr) {
    log_line("render_pixels=failed");
    return false;
  }

  uint64_t clear_pixels = 0u;
  uint64_t atlas_pixels = 0u;
  uint64_t normal_atlas_pixels = 0u;
  uint64_t specular_atlas_pixels = 0u;
  for (int y = 0; y < surface->h; ++y) {
    for (int x = 0; x < surface->w; ++x) {
      Uint8 red = 0;
      Uint8 green = 0;
      Uint8 blue = 0;
      Uint8 alpha = 0;
      if (!SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha)) {
        log_line("render_pixels=read_failed");
        SDL_DestroySurface(surface);
        return false;
      }

      if (color_matches(red, green, blue, kClearRed, kClearGreen, kClearBlue)) {
        ++clear_pixels;
      } else if (alpha != 0u) {
        ++atlas_pixels;
      }
      if (!color_matches(red, green, blue, kClearRed, kClearGreen,
                         kClearBlue) &&
          inside_rect(x, y, kMaterialAtlasProbeNormalX, kMaterialAtlasProbeY,
                      kMaterialAtlasProbeSize)) {
        ++normal_atlas_pixels;
      }
      if (!color_matches(red, green, blue, kClearRed, kClearGreen,
                         kClearBlue) &&
          inside_rect(x, y, kMaterialAtlasProbeSpecularX, kMaterialAtlasProbeY,
                      kMaterialAtlasProbeSize)) {
        ++specular_atlas_pixels;
      }
    }
  }

  SDL_DestroySurface(surface);

  if (g_log != nullptr) {
    std::fprintf(g_log, "rendered_clear_pixels=%" PRIu64 "\n", clear_pixels);
    std::fprintf(g_log, "rendered_atlas_pixels=%" PRIu64 "\n", atlas_pixels);
    std::fprintf(g_log, "rendered_normal_atlas_pixels=%" PRIu64 "\n",
                 normal_atlas_pixels);
    std::fprintf(g_log, "rendered_specular_atlas_pixels=%" PRIu64 "\n",
                 specular_atlas_pixels);
    std::fflush(g_log);
  }

  if (clear_pixels == 0u || atlas_pixels == 0u || normal_atlas_pixels == 0u ||
      specular_atlas_pixels == 0u) {
    log_line("render_pixels=empty");
    return false;
  }

  return true;
}

bool present_frame(SDL_Renderer *renderer, const BasegameAtlas &atlas,
                   const std::vector<presentation_block> &blocks) {
  if (!SDL_SetRenderDrawColor(renderer, kClearRed, kClearGreen, kClearBlue,
                              kClearAlpha)) {
    log_line("render_color=failed");
    return false;
  }

  if (!SDL_RenderClear(renderer)) {
    log_line("render_clear=failed");
    return false;
  }

  if (!draw_blocks(renderer, atlas, blocks)) {
    return false;
  }

  if (!draw_material_atlas_probe(renderer, atlas)) {
    return false;
  }

  if (read_enabled_flag(kPixelValidationFlag) &&
      !validate_render_pixels(renderer)) {
    return false;
  }

  if (!SDL_RenderPresent(renderer)) {
    log_line("render_present=failed");
    return false;
  }

  if (g_log != nullptr && !blocks.empty()) {
    std::fprintf(g_log, "presented_block_count=%zu\n", blocks.size());
    std::fflush(g_log);
  }

  return true;
}

void open_log() {
  const char *log_path = std::getenv("OCTARYN_CLIENT_APP_LOG_PATH");
  if (log_path != nullptr && log_path[0] != '\0') {
    g_log = std::fopen(log_path, "w");
  }
}

} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  open_log();
  octaryn_native_crash_diagnostics_init("octaryn-client-app");
  if (g_log != nullptr) {
    std::fprintf(g_log, "crash_marker=%s\n",
                 octaryn_native_crash_diagnostics_marker_path());
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    log_line("sdl_init=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      std::fclose(g_log);
    }
    return 2;
  }

  SDL_Window *window = SDL_CreateWindow(
      "Octaryn", kWindowWidth, kWindowHeight,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
  if (window == nullptr) {
    log_line("window_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 3;
  }

  if (!octaryn_client_window_lifecycle_show(window)) {
    log_line("window_show=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 4;
  }
  octaryn_client_window_lifecycle_finish_show(window);
  log_line("window_show=0");

  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  if (renderer == nullptr) {
    log_line("renderer_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 7;
  }
  log_line("renderer_create=0");

  BasegameAtlas atlas{};
  if (!load_basegame_module_descriptor() ||
      !load_basegame_atlas(renderer, g_log, atlas)) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 10;
  }

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;

  int result = octaryn_client_initialize(&api);
  log_result("initialize", result);
  if (result != 0) {
    destroy_basegame_atlas(atlas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 5;
  }

  if (read_enabled_flag("OCTARYN_CLIENT_APP_PRESENTATION_PROBE_SNAPSHOT")) {
    result = apply_probe_snapshot();
    log_result("presentation_probe_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_basegame_atlas(atlas);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      if (g_log != nullptr) {
        std::fclose(g_log);
      }
      return 8;
    }
  }

  std::vector<presentation_block> world_snapshot_blocks;
  if (!load_world_snapshot_blocks(world_snapshot_blocks)) {
    octaryn_client_shutdown();
    destroy_basegame_atlas(atlas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 9;
  }

  if (!world_snapshot_blocks.empty()) {
    result = apply_snapshot_blocks(world_snapshot_blocks, 2u);
    log_result("world_blocks_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_basegame_atlas(atlas);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      if (g_log != nullptr) {
        std::fclose(g_log);
      }
      return 10;
    }
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  octaryn_client_camera camera{};
  octaryn_client_camera_init(&camera,
                             OCTARYN_CLIENT_CAMERA_PROJECTION_PERSPECTIVE);
  octaryn_client_camera_update(&camera);
  std::vector<presentation_block> presentation_blocks;
  while (running) {
    pointer_motion_debug_state pointer_motion{};
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT ||
          event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (g_log != nullptr) {
          std::fprintf(g_log, "live_window_event type=%" PRIu32 "\n",
                       static_cast<uint32_t>(event.type));
          std::fflush(g_log);
        }
        running = false;
      } else if (event.type == SDL_EVENT_KEY_DOWN ||
                 event.type == SDL_EVENT_KEY_UP) {
        if (g_log != nullptr) {
          std::fprintf(g_log,
                       "live_input_event type=%" PRIu32 " scancode=%d "
                       "repeat=%d\n",
                       static_cast<uint32_t>(event.type),
                       static_cast<int>(event.key.scancode),
                       event.key.repeat ? 1 : 0);
          std::fflush(g_log);
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            !SDL_GetWindowRelativeMouseMode(window)) {
          SDL_SetWindowRelativeMouseMode(window, true);
        }
        if (g_log != nullptr) {
          std::fprintf(g_log,
                       "live_pointer_event type=%" PRIu32 " button=%u "
                       "x=%.1f y=%.1f relative=%d\n",
                       static_cast<uint32_t>(event.type),
                       static_cast<unsigned>(event.button.button),
                       event.button.x, event.button.y,
                       SDL_GetWindowRelativeMouseMode(window) ? 1 : 0);
          std::fflush(g_log);
        }
      } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (SDL_GetWindowRelativeMouseMode(window)) {
          pointer_motion.xrel += event.motion.xrel;
          pointer_motion.yrel += event.motion.yrel;
        }
        if (g_log != nullptr &&
            (event.motion.xrel != 0.0f || event.motion.yrel != 0.0f)) {
          std::fprintf(g_log,
                       "live_pointer_motion xrel=%.3f yrel=%.3f relative=%d\n",
                       event.motion.xrel, event.motion.yrel,
                       SDL_GetWindowRelativeMouseMode(window) ? 1 : 0);
          std::fflush(g_log);
        }
      } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                 event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        if (g_log != nullptr) {
          std::fprintf(g_log,
                       "live_window_size type=%" PRIu32 " width=%d height=%d\n",
                       static_cast<uint32_t>(event.type), event.window.data1,
                       event.window.data2);
          std::fflush(g_log);
        }
      }
    }

    const uint64_t current_ticks = SDL_GetTicksNS();
    double delta_seconds = frame_delta_seconds(previous_ticks, current_ticks);
    if (read_enabled_flag(kInputProbeFlag) && frame_index == 0u) {
      delta_seconds = kDefaultDeltaSeconds;
    }
    octaryn_host_frame_snapshot frame =
        create_frame(frame_index + 1u, delta_seconds);
    client_input_debug_state input = read_client_input(window, pointer_motion);
    apply_input_probe(input, frame.timing.frame_index);
    if (!update_client_camera(renderer, camera, input,
                              frame.timing.delta_seconds)) {
      result = -4;
      running = false;
      break;
    }
    apply_input_to_frame(frame, input, camera);
    previous_ticks = current_ticks;
    log_client_tick_input_frame(frame);

    result = octaryn_client_tick(&frame);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    uint32_t drained_updates = 0u;
    if (!drain_presentation_updates(presentation_blocks, drained_updates)) {
      result = -3;
      running = false;
      break;
    }
    log_live_client_frame(frame.timing.frame_index, input, camera,
                          drained_updates, presentation_blocks);

    if (!present_frame(renderer, atlas, presentation_blocks)) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      }
      result = -2;
      running = false;
      break;
    }

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }

    SDL_Delay(1u);
  }

  octaryn_client_shutdown();
  log_line("shutdown=0");
  destroy_basegame_atlas(atlas);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (g_log != nullptr) {
    std::fclose(g_log);
  }

  return result == 0 ? 0 : 6;
}
