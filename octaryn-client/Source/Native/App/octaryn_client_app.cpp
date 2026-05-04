#include "octaryn_client_asset_path.h"
#include "octaryn_client_basegame_atlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_chunk_view.h"
#include "octaryn_client_fly_player_controller.h"
#include "octaryn_client_host_exports.h"
#include "octaryn_client_player_control_input.h"
#include "octaryn_client_shader_creation.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_native_crash_diagnostics.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <limits>
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

struct server_chunk_stream_column_record {
  int32_t chunkX;
  int32_t chunkZ;
  int32_t originX;
  int32_t originZ;
  uint32_t blockOffset;
  uint32_t blockCount;
};

struct server_chunk_stream_file {
  int32_t version = 0;
  uint64_t epoch = 0u;
  std::string source;
  int32_t centerChunkX = 0;
  int32_t centerChunkZ = 0;
  uint32_t radius = 0u;
  uint64_t worldTimeDayIndex = 0u;
  uint32_t worldTimeSecondOfDay = 0u;
  double worldTimeTotalSeconds = 0.0;
  float worldTimeDayFraction = 0.5f;
  std::vector<server_chunk_stream_column_record> columns;
  std::vector<world_block_record> blocks;
};

struct graphics_shader_metadata_file {
  uint32_t samplers = 0u;
  uint32_t storage_textures = 0u;
  uint32_t storage_buffers = 0u;
  uint32_t uniform_buffers = 0u;
};

struct client_chunk_view_intent_file {
  int32_t version = 1;
  uint64_t epoch = 0u;
  int32_t centerChunkX = 0;
  int32_t centerChunkZ = 0;
  uint32_t radius = 0u;
};

struct client_player_input_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  double deltaSeconds = 0.0;
  uint32_t flags = 0u;
  uint32_t controller = 0u;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float moveZ = 0.0f;
  float cameraX = 0.0f;
  float cameraY = 0.0f;
  float cameraZ = 0.0f;
  float cameraPitch = 0.0f;
  float cameraYaw = 0.0f;
  int32_t relativeMouse = 0;
};

struct client_block_interaction_command_file {
  uint64_t requestId = 0u;
  int32_t editX = 0;
  int32_t editY = 0;
  int32_t editZ = 0;
  uint16_t block = 0u;
  float cameraX = 0.0f;
  float cameraY = 0.0f;
  float cameraZ = 0.0f;
  int32_t hitX = 0;
  int32_t hitY = 0;
  int32_t hitZ = 0;
};

struct client_block_interaction_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  std::vector<client_block_interaction_command_file> commands;
};

} // namespace octaryn_client_app

namespace {

using octaryn::client::rendering::basegame_atlas_top_layer_for_block;
using octaryn::client::rendering::BasegameAtlas;
using octaryn::client::rendering::create_graphics_shader;
using octaryn::client::rendering::destroy_basegame_atlas;
using octaryn::client::rendering::GraphicsShaderMetadata;
using octaryn::client::rendering::load_basegame_atlas;
using octaryn_client_app::world_block_file;
using octaryn_client_app::world_block_record;
using octaryn_client_app::client_chunk_view_intent_file;
using octaryn_client_app::client_block_interaction_command_file;
using octaryn_client_app::client_block_interaction_intent_file;
using octaryn_client_app::client_player_input_intent_file;
using octaryn_client_app::graphics_shader_metadata_file;
using octaryn_client_app::server_chunk_stream_file;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr Uint8 kClearRed = 18;
constexpr Uint8 kClearGreen = 43;
constexpr Uint8 kClearBlue = 49;
constexpr Uint8 kClearAlpha = 255;
constexpr int kBlockDrawSize = 48;
constexpr int kWorldBlockDrawSize = 8;
constexpr float kRenderDistanceBlocks = 42.0f;
constexpr int kMaterialAtlasProbeLayer = 16;
constexpr int kMaterialAtlasProbeY = 8;
constexpr int kMaterialAtlasProbeNormalX = 8;
constexpr int kMaterialAtlasProbeSpecularX = 40;
constexpr int kMaterialAtlasProbeSize = 24;
constexpr float kPi = 3.14159265358979323846f;
constexpr int kWorldSnapshotMinX = 0;
constexpr int kWorldSnapshotMaxXExclusive = 32;
constexpr int kWorldSnapshotMinZ = 0;
constexpr int kWorldSnapshotMaxZExclusive = 32;
constexpr int kMaxPresentationUpdatesPerFrame = 256;
constexpr uint32_t kMaxChunkMeshUploadsPerFrame = 1024u;
constexpr uint32_t kMaxPackedOpaqueFacesPerFrame = 1048576u;
constexpr uint32_t kMaxPackedTransparentFacesPerFrame = 262144u;
constexpr uint32_t kMaxPackedSpriteVerticesPerFrame = 262144u;
constexpr int kProcessChunkStreamRadius = 0;
constexpr float kFlySpeedBlocksPerSecond = 10.0f;
constexpr float kFlyFastSpeedBlocksPerSecond = 100.0f;
constexpr float kMouseSensitivityDegrees = 0.1f;
constexpr const char *kInputProbeFlag = "OCTARYN_CLIENT_APP_INPUT_PROBE";
constexpr const char *kPixelValidationFlag =
    "OCTARYN_CLIENT_APP_VALIDATE_PIXELS";
constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts kJsonWriteOptions{.prettify = true};
constexpr uint32_t kInputJumpFlag = 1u << 0u;
constexpr uint32_t kInputSprintFlag = 1u << 1u;
constexpr uint32_t kInputFlyModeFlag = 1u << 2u;
constexpr uint32_t kInputPrimaryFlag = 1u << 3u;
constexpr uint32_t kInputSecondaryFlag = 1u << 4u;
constexpr uint32_t kHostCommandCriticalFlag = 1u;
constexpr uint32_t kHostCommandClientInteractionFlag = 1u << 1u;
constexpr uint32_t kDrawFlagUseFaceBuffer = 1u << 1u;
constexpr uint16_t kDefaultInteractionPlaceBlock = 29u;
constexpr float kBlockInteractionReachBlocks = 6.0f;
constexpr float kBlockInteractionRayStepBlocks = 0.05f;

FILE *g_log;

struct presentation_block {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct block_position_key {
  int32_t x;
  int32_t y;
  int32_t z;

  bool operator==(const block_position_key &other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct block_position_key_hash {
  size_t operator()(const block_position_key &key) const {
    uint64_t value = static_cast<uint32_t>(key.x);
    value = value * 1099511628211ull ^ static_cast<uint32_t>(key.y);
    value = value * 1099511628211ull ^ static_cast<uint32_t>(key.z);
    return static_cast<size_t>(value);
  }
};

using block_lookup =
    std::unordered_map<block_position_key, uint16_t, block_position_key_hash>;

struct client_block_raycast_hit {
  bool has_hit = false;
  block_position_key hit{};
  block_position_key adjacent{};
  uint16_t block = 0u;
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

using client_key_state = std::array<bool, SDL_SCANCODE_COUNT>;

struct client_command_frame_counts {
  uint32_t enqueued = 0u;
  uint32_t set_block = 0u;
  uint32_t place_block = 0u;
  uint32_t break_block = 0u;
};

struct world_mesh_upload_frame {
  std::vector<octaryn_client_chunk_mesh_upload_record> chunks;
  std::vector<uint64_t> opaque_faces;
  std::vector<uint64_t> transparent_faces;
  std::vector<uint32_t> sprite_vertices;
  uint32_t fluid_blocks = 0u;
  uint64_t opaque_bytes = 0u;
  uint64_t transparent_bytes = 0u;
  uint64_t sprite_bytes = 0u;
};

struct world_mesh_upload_scratch {
  std::vector<octaryn_client_chunk_mesh_upload_record> chunks;
  std::vector<uint64_t> opaque_faces;
  std::vector<uint64_t> transparent_faces;
  std::vector<uint32_t> sprite_vertices;
};

struct world_mesh_gpu_buffers {
  SDL_GPUBuffer *opaque_faces = nullptr;
  SDL_GPUBuffer *transparent_faces = nullptr;
  SDL_GPUBuffer *sprite_vertices = nullptr;
};

struct compiled_graphics_shader {
  GraphicsShaderMetadata metadata{};
  std::vector<uint8_t> code;
};

struct client_shader_pipelines {
  SDL_GPUGraphicsPipeline *sky = nullptr;
  SDL_GPUGraphicsPipeline *world = nullptr;
  SDL_GPUSampler *atlas_sampler = nullptr;
};

struct server_world_time_state {
  bool active = false;
  uint64_t day_index = 0u;
  uint32_t second_of_day = 43200u;
  double total_seconds = 43200.0;
  float day_fraction = 0.5f;
};

struct matrix_uniform {
  float values[4][4]{};
};

struct sky_uniforms {
  float light_direction_and_sky_visibility[4]{};
  float twilight_celestial_cloud_time[4]{};
  float camera_position_and_cloud_height[4]{};
  float celestial_toggles[4]{};
};

struct chunk_uniforms {
  int32_t chunk_position[2]{};
  uint32_t face_offset = 0u;
  uint32_t draw_flags = 0u;
};

struct camera_uniforms {
  float position[4]{};
};

client_command_frame_counts g_command_frame_counts;
bool g_gpu_path_logged;

bool window_output_size(SDL_Window *window, int *width, int *height);

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

bool key_down(const client_key_state &keys, SDL_Scancode scancode) {
  return keys[scancode];
}

client_input_debug_state
read_client_input(SDL_Window *window,
                  const pointer_motion_debug_state &pointer_motion,
                  const client_key_state &keys) {
  client_input_debug_state input{};
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

const char *command_edit_label(const octaryn_host_command &command) {
  if (command.kind != 1u) {
    return "none";
  }

  return command.d == 0 ? "break" : "place";
}

void reset_command_frame_counts() { g_command_frame_counts = {}; }

void count_enqueued_command(const octaryn_host_command &command) {
  ++g_command_frame_counts.enqueued;
  if (command.kind != 1u) {
    return;
  }

  ++g_command_frame_counts.set_block;
  if (command.d == 0) {
    ++g_command_frame_counts.break_block;
  } else {
    ++g_command_frame_counts.place_block;
  }
}

void log_client_command_enqueue(const octaryn_host_command &command) {
  if (g_log == nullptr) {
    return;
  }

  std::fprintf(g_log,
               "live_client_command_enqueue kind=%" PRIu32 " request=%" PRIu64
               " target=%" PRIu64 " edit=%s block=(%" PRId32 ",%" PRId32
               ",%" PRId32 ",%" PRId32 ") flags=%" PRIu32 "\n",
               command.kind, command.request_id, command.target_id,
               command_edit_label(command), command.a, command.b, command.c,
               command.d, command.flags);
  std::fflush(g_log);
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command *command) {
  if (command != nullptr) {
    count_enqueued_command(*command);
  }

  if (command != nullptr) {
    log_client_command_enqueue(*command);
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

void fill_player_control_input(
    octaryn_client_player_control_input &control_input,
    const client_input_debug_state &input,
    const octaryn_client_fly_player_controller &controller) {
  octaryn_client_player_control_input_clear(&control_input);
  control_input.move_right = input.move_x > 0.0f ? 1 : 0;
  control_input.move_left = input.move_x < 0.0f ? 1 : 0;
  control_input.move_up = input.move_y > 0.0f ? 1 : 0;
  control_input.move_down = input.move_y < 0.0f ? 1 : 0;
  control_input.move_forward = input.move_z > 0.0f ? 1 : 0;
  control_input.move_backward = input.move_z < 0.0f ? 1 : 0;
  control_input.sprint = (input.flags & kInputSprintFlag) != 0u ? 1 : 0;
  const float sensitivity = controller.mouse_sensitivity_degrees_per_pixel;
  if (sensitivity > 0.0f) {
    control_input.mouse_yaw_delta = input.look_yaw / sensitivity;
    control_input.mouse_pitch_delta = -input.look_pitch / sensitivity;
  }
}

bool update_client_player_controller(
    SDL_Window *window,
    octaryn_client_fly_player_controller &controller,
    const client_input_debug_state &input,
    double delta_seconds) {
  int render_width = 0;
  int render_height = 0;
  if (!window_output_size(window, &render_width, &render_height)) {
    return false;
  }

  octaryn_client_fly_player_controller_resize_viewport(
      &controller, render_width, render_height);
  octaryn_client_player_control_input control_input{};
  fill_player_control_input(control_input, input, controller);
  octaryn_client_fly_player_controller_update(
      &controller, &control_input, static_cast<float>(delta_seconds));
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

bool read_binary_file(const char *path, const char *failure_label,
                      std::vector<uint8_t> &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool build_client_bundle_path(char *path, size_t path_size,
                              const char *relative_path,
                              const char *failure_label) {
  if (!octaryn_client_bundle_path_build(path, path_size, relative_path)) {
    log_line(failure_label);
    return false;
  }
  return true;
}

bool load_graphics_shader_asset(const char *shader_name,
                                compiled_graphics_shader &shader) {
  char metadata_path[4096] = {};
  char spirv_path[4096] = {};
  std::string relative_metadata = "Client/Shaders/Compiled/";
  relative_metadata += shader_name;
  relative_metadata += ".json";
  std::string relative_spirv = "Client/Shaders/Compiled/";
  relative_spirv += shader_name;
  relative_spirv += ".spv";
  if (!build_client_bundle_path(metadata_path, sizeof(metadata_path),
                                relative_metadata.c_str(),
                                "client_shader_metadata_path=failed") ||
      !build_client_bundle_path(spirv_path, sizeof(spirv_path),
                                relative_spirv.c_str(),
                                "client_shader_spirv_path=failed")) {
    return false;
  }

  std::string metadata_payload;
  if (!read_text_file(metadata_path, "client_shader_metadata=open_failed",
                      metadata_payload) ||
      !read_binary_file(spirv_path, "client_shader_spirv=open_failed",
                        shader.code)) {
    return false;
  }

  graphics_shader_metadata_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, metadata_payload);
  if (error) {
    log_line("client_shader_metadata=parse_failed");
    return false;
  }

  shader.metadata.samplers = file.samplers;
  shader.metadata.storageTextures = file.storage_textures;
  shader.metadata.storageBuffers = file.storage_buffers;
  shader.metadata.uniformBuffers = file.uniform_buffers;
  return !shader.code.empty();
}

SDL_GPUGraphicsPipeline *create_swapchain_pipeline(
    SDL_GPUDevice *device, SDL_GPUTextureFormat color_format,
    const char *vertex_shader_name, const char *fragment_shader_name) {
  compiled_graphics_shader vertex_shader{};
  compiled_graphics_shader fragment_shader{};
  if (!load_graphics_shader_asset(vertex_shader_name, vertex_shader) ||
      !load_graphics_shader_asset(fragment_shader_name, fragment_shader)) {
    return nullptr;
  }

  SDL_GPUShader *vertex = create_graphics_shader(
      device, vertex_shader.metadata, vertex_shader.code.data(),
      vertex_shader.code.size(), "main", SDL_GPU_SHADERFORMAT_SPIRV,
      SDL_GPU_SHADERSTAGE_VERTEX);
  SDL_GPUShader *fragment = create_graphics_shader(
      device, fragment_shader.metadata, fragment_shader.code.data(),
      fragment_shader.code.size(), "main", SDL_GPU_SHADERFORMAT_SPIRV,
      SDL_GPU_SHADERSTAGE_FRAGMENT);
  if (vertex == nullptr || fragment == nullptr) {
    if (vertex != nullptr) {
      SDL_ReleaseGPUShader(device, vertex);
    }
    if (fragment != nullptr) {
      SDL_ReleaseGPUShader(device, fragment);
    }
    log_line("client_shader=create_failed");
    return nullptr;
  }

  SDL_GPUColorTargetDescription color_target{};
  color_target.format = color_format;

  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vertex;
  info.fragment_shader = fragment;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  info.target_info.color_target_descriptions = &color_target;
  info.target_info.num_color_targets = 1u;
  SDL_GPUGraphicsPipeline *pipeline =
      SDL_CreateGPUGraphicsPipeline(device, &info);
  SDL_ReleaseGPUShader(device, vertex);
  SDL_ReleaseGPUShader(device, fragment);
  if (pipeline == nullptr) {
    log_line("client_graphics_pipeline=create_failed");
  }
  return pipeline;
}

bool initialize_shader_pipelines(SDL_GPUDevice *device, SDL_Window *window,
                                 client_shader_pipelines &pipelines) {
  const SDL_GPUTextureFormat swapchain_format =
      SDL_GetGPUSwapchainTextureFormat(device, window);
  pipelines.sky = create_swapchain_pipeline(device, swapchain_format,
                                            "sky.vert", "sky.frag");
  pipelines.world = create_swapchain_pipeline(device, swapchain_format,
                                              "opaque_packed.vert",
                                              "world_mesh.frag");

  SDL_GPUSamplerCreateInfo sampler_info{};
  sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  pipelines.atlas_sampler = SDL_CreateGPUSampler(device, &sampler_info);

  if (pipelines.sky == nullptr || pipelines.world == nullptr ||
      pipelines.atlas_sampler == nullptr) {
    log_line("live_shader_pipeline active=0 reason=create_failed");
    return false;
  }

  log_line("live_shader_pipeline active=1 sky=1 world=1 source=compiled_spirv");
  return true;
}

void release_shader_pipelines(SDL_GPUDevice *device,
                              client_shader_pipelines &pipelines) {
  if (pipelines.atlas_sampler != nullptr) {
    SDL_ReleaseGPUSampler(device, pipelines.atlas_sampler);
    pipelines.atlas_sampler = nullptr;
  }
  if (pipelines.world != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.world);
    pipelines.world = nullptr;
  }
  if (pipelines.sky != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.sky);
    pipelines.sky = nullptr;
  }
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

bool apply_top_blocks_from_records(const std::vector<world_block_record> &records,
                                   bool spawn_only,
                                   std::vector<presentation_block> &blocks) {
  std::unordered_map<uint64_t, presentation_block> top_blocks;
  for (const world_block_record &record : records) {
    if (record.block == 0u || (spawn_only && !is_spawn_column_block(record))) {
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
  return !blocks.empty();
}

bool apply_blocks_from_records(const std::vector<world_block_record> &records,
                               bool spawn_only,
                               std::vector<presentation_block> &blocks) {
  blocks.clear();
  blocks.reserve(records.size());
  for (const world_block_record &record : records) {
    if (record.block == 0u || (spawn_only && !is_spawn_column_block(record))) {
      continue;
    }

    blocks.push_back(
        presentation_block{record.x, record.y, record.z, record.block});
  }

  std::sort(
      blocks.begin(), blocks.end(),
      [](const presentation_block &left, const presentation_block &right) {
        if (left.x != right.x) {
          return left.x < right.x;
        }
        if (left.z != right.z) {
          return left.z < right.z;
        }

        return left.y < right.y;
      });
  return !blocks.empty();
}

bool load_world_snapshot_blocks(std::vector<presentation_block> &snapshot_blocks,
                                std::vector<presentation_block> &surface_blocks,
                                server_world_time_state &world_time) {
  const char *stream_path = std::getenv("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH");
  if (stream_path != nullptr && stream_path[0] != '\0') {
    std::string stream_payload;
    if (!read_text_file(stream_path, "server_chunk_stream_file=open_failed",
                        stream_payload)) {
      return false;
    }

    server_chunk_stream_file stream{};
    const auto stream_error = glz::read<kJsonReadOptions>(stream, stream_payload);
    if (stream_error) {
      log_line("server_chunk_stream_file=parse_failed");
      return false;
    }

    if (stream.version != 1 || stream.source != "server_process_chunk_stream") {
      log_line("server_chunk_stream_file=unsupported_version");
      return false;
    }

    apply_blocks_from_records(stream.blocks, false, snapshot_blocks);
    apply_top_blocks_from_records(stream.blocks, false, surface_blocks);
    world_time.active = true;
    world_time.day_index = stream.worldTimeDayIndex;
    world_time.second_of_day = stream.worldTimeSecondOfDay;
    world_time.total_seconds = stream.worldTimeTotalSeconds;
    world_time.day_fraction =
        std::clamp(stream.worldTimeDayFraction, 0.0f, 1.0f);
    if (g_log != nullptr) {
      std::fprintf(g_log, "server_chunk_stream_loaded=%zu\n",
                   stream.blocks.size());
      std::fprintf(g_log, "server_chunk_stream_columns=%zu\n",
                   stream.columns.size());
      std::fprintf(g_log, "server_chunk_stream_surface_blocks_applied=%zu\n",
                   surface_blocks.size());
      std::fprintf(g_log,
                   "live_chunk_streaming active=1 source=server_process "
                   "epoch=%" PRIu64 " center=(%d,%d) radius=%" PRIu32
                   " columns=%zu loaded=%zu surface_blocks=%zu\n",
                   stream.epoch, stream.centerChunkX, stream.centerChunkZ,
                   stream.radius, stream.columns.size(), stream.blocks.size(),
                   surface_blocks.size());
      std::fprintf(g_log,
                   "live_sky_uniforms source=server_process day_fraction=%.6f "
                   "day_index=%" PRIu64 " second_of_day=%" PRIu32
                   " total_seconds=%.3f\n",
                   world_time.day_fraction, world_time.day_index,
                   world_time.second_of_day, world_time.total_seconds);
      std::fflush(g_log);
    }
    return !snapshot_blocks.empty();
  }

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

  apply_blocks_from_records(file.blocks, true, snapshot_blocks);
  apply_top_blocks_from_records(file.blocks, true, surface_blocks);

  if (g_log != nullptr) {
    std::fprintf(g_log, "world_blocks_loaded=%zu\n", file.blocks.size());
    std::fprintf(g_log, "world_surface_blocks_applied=%zu\n",
                 surface_blocks.size());
    std::fprintf(g_log,
                 "live_chunk_streaming active=0 source=world_blocks_path "
                 "loaded=%zu surface_blocks=%zu reason=static_snapshot\n",
                 file.blocks.size(), surface_blocks.size());
    std::fflush(g_log);
  }
  return !snapshot_blocks.empty();
}

bool write_chunk_view_intent(const octaryn_client_chunk_view &view,
                             uint64_t epoch) {
  const char *path = std::getenv("OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    return true;
  }

  client_chunk_view_intent_file intent{};
  intent.epoch = epoch;
  intent.centerChunkX = view.origin_x + view.width / 2;
  intent.centerChunkZ = view.origin_z + view.width / 2;
  intent.radius = kProcessChunkStreamRadius;

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_chunk_view_intent_write=encode_failed");
    return false;
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path parent = output_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    log_line("live_chunk_view_intent_write=open_failed");
    return false;
  }

  file.write(output.data(), static_cast<std::streamsize>(output.size()));
  if (!file.good()) {
    log_line("live_chunk_view_intent_write=failed");
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_view_intent source=process_file path=%s "
                 "epoch=%" PRIu64 " center=(%d,%d) radius=%" PRIu32 "\n",
                 path, intent.epoch, intent.centerChunkX, intent.centerChunkZ,
                 intent.radius);
    std::fflush(g_log);
  }
  return true;
}

bool write_player_input_intent(const octaryn_host_frame_snapshot &frame) {
  const char *path = std::getenv("OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    return true;
  }

  if (read_enabled_flag(kInputProbeFlag) && frame.timing.frame_index != 1u) {
    return true;
  }

  const bool has_intent =
      frame.input.move_x != 0.0f || frame.input.move_y != 0.0f ||
      frame.input.move_z != 0.0f || frame.input.relative_mouse != 0 ||
      (frame.input.flags & (kInputJumpFlag | kInputSprintFlag |
                            kInputPrimaryFlag | kInputSecondaryFlag)) != 0u;
  if (!has_intent) {
    return true;
  }

  client_player_input_intent_file intent{};
  intent.frameIndex = frame.timing.frame_index;
  intent.deltaSeconds = frame.timing.delta_seconds;
  intent.flags = frame.input.flags;
  intent.controller = frame.input.controller;
  intent.moveX = frame.input.move_x;
  intent.moveY = frame.input.move_y;
  intent.moveZ = frame.input.move_z;
  intent.cameraX = frame.input.camera_x;
  intent.cameraY = frame.input.camera_y;
  intent.cameraZ = frame.input.camera_z;
  intent.cameraPitch = frame.input.camera_pitch;
  intent.cameraYaw = frame.input.camera_yaw;
  intent.relativeMouse = frame.input.relative_mouse;

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_player_input_intent_write=encode_failed");
    return false;
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path parent = output_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    log_line("live_player_input_intent_write=open_failed");
    return false;
  }

  file.write(output.data(), static_cast<std::streamsize>(output.size()));
  if (!file.good()) {
    log_line("live_player_input_intent_write=failed");
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_player_input_intent source=process_file path=%s "
                 "frame=%" PRIu64 " flags=%" PRIu32
                 " controller=%" PRIu32 " move=(%.3f,%.3f,%.3f) "
                 "camera=(%.3f,%.3f,%.3f,%.6f,%.6f)\n",
                 path, frame.timing.frame_index, frame.input.flags,
                 frame.input.controller, frame.input.move_x, frame.input.move_y,
                 frame.input.move_z, frame.input.camera_x, frame.input.camera_y,
                 frame.input.camera_z, frame.input.camera_pitch,
                 frame.input.camera_yaw);
    std::fflush(g_log);
  }
  return true;
}

block_lookup build_block_lookup(const std::vector<presentation_block> &blocks) {
  block_lookup lookup;
  lookup.reserve(blocks.size());
  for (const presentation_block &block : blocks) {
    if (block.block == 0u) {
      continue;
    }

    lookup[block_position_key{block.x, block.y, block.z}] = block.block;
  }

  return lookup;
}

uint16_t find_block(const block_lookup &lookup, const block_position_key &key) {
  const auto iterator = lookup.find(key);
  return iterator == lookup.end() ? 0u : iterator->second;
}

block_position_key block_position_at(float x, float y, float z) {
  return block_position_key{
      static_cast<int32_t>(std::floor(x)),
      static_cast<int32_t>(std::floor(y)),
      static_cast<int32_t>(std::floor(z)),
  };
}

client_block_raycast_hit raycast_block_interaction(
    const octaryn_client_camera &camera,
    const std::vector<presentation_block> &blocks) {
  const block_lookup lookup = build_block_lookup(blocks);
  if (lookup.empty()) {
    return {};
  }

  float direction_x = 0.0f;
  float direction_y = 0.0f;
  float direction_z = 0.0f;
  octaryn_client_camera_forward_vector(&camera, &direction_x, &direction_y,
                                       &direction_z);

  block_position_key previous = block_position_at(
      camera.position[0], camera.position[1], camera.position[2]);
  for (float distance = kBlockInteractionRayStepBlocks;
       distance <= kBlockInteractionReachBlocks;
       distance += kBlockInteractionRayStepBlocks) {
    const block_position_key current = block_position_at(
        camera.position[0] + direction_x * distance,
        camera.position[1] + direction_y * distance,
        camera.position[2] + direction_z * distance);
    const uint16_t block = find_block(lookup, current);
    if (block != 0u) {
      return client_block_raycast_hit{
          true,
          current,
          previous == current ? block_position_key{current.x, current.y + 1, current.z}
                              : previous,
          block,
      };
    }

    previous = current;
  }

  return {};
}

client_block_interaction_command_file make_block_interaction_command(
    uint64_t request_id,
    const block_position_key &edit,
    uint16_t block,
    const octaryn_client_camera &camera,
    const block_position_key &hit) {
  return client_block_interaction_command_file{
      request_id,
      edit.x,
      edit.y,
      edit.z,
      block,
      camera.position[0],
      camera.position[1],
      camera.position[2],
      hit.x,
      hit.y,
      hit.z,
  };
}

octaryn_host_command make_logged_interaction_command(
    const client_block_interaction_command_file &source) {
  octaryn_host_command command{};
  command.version = 1u;
  command.size = OCTARYN_HOST_COMMAND_SIZE;
  command.kind = 1u;
  command.flags = kHostCommandCriticalFlag | kHostCommandClientInteractionFlag;
  command.request_id = source.requestId;
  command.a = source.editX;
  command.b = source.editY;
  command.c = source.editZ;
  command.d = source.block;
  command.x = source.cameraX;
  command.y = source.cameraY;
  command.z = source.cameraZ;
  command.x2 = static_cast<float>(source.hitX);
  command.y2 = static_cast<float>(source.hitY);
  command.z2 = static_cast<float>(source.hitZ);
  return command;
}

bool write_block_interaction_intent(
    const octaryn_host_frame_snapshot &frame,
    const client_input_debug_state &input,
    const octaryn_client_camera &camera,
    const std::vector<presentation_block> &blocks) {
  const char *path =
      std::getenv("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    return true;
  }

  const bool primary = (input.flags & kInputPrimaryFlag) != 0u;
  const bool secondary = (input.flags & kInputSecondaryFlag) != 0u;
  if (!primary && !secondary) {
    return true;
  }

  const client_block_raycast_hit hit =
      raycast_block_interaction(camera, blocks);
  if (!hit.has_hit) {
    log_line("live_block_interaction_intent active=0 reason=raycast_miss");
    return true;
  }

  client_block_interaction_intent_file intent{};
  intent.frameIndex = frame.timing.frame_index;
  const uint64_t request_base = frame.timing.frame_index * 2u;
  if (primary) {
    intent.commands.push_back(make_block_interaction_command(
        request_base + 1u, hit.hit, 0u, camera, hit.hit));
  }
  if (secondary) {
    const block_position_key place_edit = primary ? hit.hit : hit.adjacent;
    const block_position_key place_hit =
        primary ? block_position_key{hit.hit.x, hit.hit.y - 1, hit.hit.z}
                : hit.hit;
    intent.commands.push_back(make_block_interaction_command(
        request_base + 2u, place_edit, kDefaultInteractionPlaceBlock, camera,
        place_hit));
  }

  for (const client_block_interaction_command_file &command_file :
       intent.commands) {
    const octaryn_host_command command =
        make_logged_interaction_command(command_file);
    count_enqueued_command(command);
    log_client_command_enqueue(command);
  }

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_block_interaction_intent_write=encode_failed");
    return false;
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path parent = output_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    log_line("live_block_interaction_intent_write=open_failed");
    return false;
  }

  file.write(output.data(), static_cast<std::streamsize>(output.size()));
  if (!file.good()) {
    log_line("live_block_interaction_intent_write=failed");
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_block_interaction_intent source=process_file path=%s "
                 "frame=%" PRIu64 " commands=%zu break=%d place=%d "
                 "hit=(%d,%d,%d,%u) adjacent=(%d,%d,%d)\n",
                 path, frame.timing.frame_index, intent.commands.size(),
                 primary ? 1 : 0, secondary ? 1 : 0, hit.hit.x, hit.hit.y,
                 hit.hit.z, hit.block, hit.adjacent.x, hit.adjacent.y,
                 hit.adjacent.z);
    std::fflush(g_log);
  }
  return true;
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

bool drain_chunk_mesh_uploads(uint64_t frame_index,
                              world_mesh_upload_scratch &scratch,
                              world_mesh_upload_frame &upload_frame) {
  uint32_t upload_written = 0u;
  uint32_t opaque_faces_written = 0u;
  uint32_t transparent_faces_written = 0u;
  uint32_t sprite_vertices_written = 0u;
  const int result = octaryn_client_drain_chunk_mesh_uploads(
      scratch.chunks.data(), static_cast<uint32_t>(scratch.chunks.size()),
      &upload_written, scratch.opaque_faces.data(),
      static_cast<uint32_t>(scratch.opaque_faces.size()), &opaque_faces_written,
      scratch.transparent_faces.data(),
      static_cast<uint32_t>(scratch.transparent_faces.size()),
      &transparent_faces_written, scratch.sprite_vertices.data(),
      static_cast<uint32_t>(scratch.sprite_vertices.size()),
      &sprite_vertices_written);
  if (result != 0) {
    log_result("drain_chunk_mesh_uploads", result);
    return false;
  }

  upload_frame.chunks.assign(scratch.chunks.begin(),
                             scratch.chunks.begin() + upload_written);
  upload_frame.opaque_faces.assign(
      scratch.opaque_faces.begin(),
      scratch.opaque_faces.begin() + opaque_faces_written);
  upload_frame.transparent_faces.assign(
      scratch.transparent_faces.begin(),
      scratch.transparent_faces.begin() + transparent_faces_written);
  upload_frame.sprite_vertices.assign(
      scratch.sprite_vertices.begin(),
      scratch.sprite_vertices.begin() + sprite_vertices_written);
  upload_frame.fluid_blocks = 0u;
  upload_frame.opaque_bytes = 0u;
  upload_frame.transparent_bytes = 0u;
  upload_frame.sprite_bytes = 0u;
  uint32_t sprite_indices = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       upload_frame.chunks) {
    upload_frame.fluid_blocks += chunk.fluid_block_count;
    upload_frame.opaque_bytes += chunk.opaque_byte_count;
    upload_frame.transparent_bytes += chunk.transparent_byte_count;
    upload_frame.sprite_bytes += chunk.sprite_byte_count;
    sprite_indices += chunk.sprite_index_count;
  }

  if (upload_written != 0u && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_mesh_plan frame=%" PRIu64
                 " active=1 source=managed_presentation_pipeline"
                 " dirty_chunks=%" PRIu32 " opaque_faces=%" PRIu32
                 " transparent_faces=%" PRIu32 " sprite_vertices=%" PRIu32
                 " sprite_indices=%" PRIu32 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, upload_written, opaque_faces_written,
                 transparent_faces_written, sprite_vertices_written,
                 sprite_indices, upload_frame.fluid_blocks);
    std::fflush(g_log);
  }
  return true;
}

void release_world_mesh_gpu_buffers(SDL_GPUDevice *device,
                                    world_mesh_gpu_buffers &buffers) {
  if (buffers.opaque_faces != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.opaque_faces);
    buffers.opaque_faces = nullptr;
  }
  if (buffers.transparent_faces != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.transparent_faces);
    buffers.transparent_faces = nullptr;
  }
  if (buffers.sprite_vertices != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.sprite_vertices);
    buffers.sprite_vertices = nullptr;
  }
}

bool upload_gpu_buffer(SDL_GPUDevice *device, const void *data,
                       uint64_t byte_count, SDL_GPUBufferUsageFlags usage,
                       const char *log_prefix, SDL_GPUBuffer *&target) {
  if (target != nullptr) {
    SDL_ReleaseGPUBuffer(device, target);
    target = nullptr;
  }
  if (byte_count == 0u) {
    return true;
  }
  if (byte_count > std::numeric_limits<Uint32>::max()) {
    log_line("gpu_chunk_mesh_upload=too_large");
    return false;
  }

  SDL_GPUBufferCreateInfo buffer_info{};
  buffer_info.usage = usage;
  buffer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buffer_info);
  if (buffer == nullptr) {
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_buffer=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    SDL_ReleaseGPUBuffer(device, buffer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_transfer=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_transfer=map_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }
  std::memcpy(mapped, data, static_cast<size_t>(byte_count));
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_command=create_failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  SDL_GPUTransferBufferLocation source{};
  source.transfer_buffer = transfer;
  SDL_GPUBufferRegion destination{};
  destination.buffer = buffer;
  destination.size = static_cast<Uint32>(byte_count);
  SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
  SDL_EndGPUCopyPass(copy_pass);

  const bool submitted = SDL_SubmitGPUCommandBuffer(command_buffer);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  if (!submitted || !SDL_WaitForGPUIdle(device)) {
    SDL_ReleaseGPUBuffer(device, buffer);
    if (g_log != nullptr) {
      std::fprintf(g_log, "%s_upload=failed\n", log_prefix);
      std::fflush(g_log);
    }
    return false;
  }

  target = buffer;
  return true;
}

bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index) {
  if (upload_frame.chunks.empty()) {
    return true;
  }

  if (!upload_gpu_buffer(
          device, upload_frame.opaque_faces.data(), upload_frame.opaque_bytes,
          SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, "gpu_chunk_mesh_opaque",
          buffers.opaque_faces) ||
      !upload_gpu_buffer(device, upload_frame.transparent_faces.data(),
                         upload_frame.transparent_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_transparent",
                         buffers.transparent_faces) ||
      !upload_gpu_buffer(device, upload_frame.sprite_vertices.data(),
                         upload_frame.sprite_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_sprite", buffers.sprite_vertices)) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_mesh_upload frame=%" PRIu64
                 " active=1 target=sdl_gpu chunks=%zu opaque_bytes=%" PRIu64
                 " transparent_bytes=%" PRIu64 " sprite_bytes=%" PRIu64
                 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, upload_frame.chunks.size(),
                 upload_frame.opaque_bytes, upload_frame.transparent_bytes,
                 upload_frame.sprite_bytes, upload_frame.fluid_blocks);
    std::fflush(g_log);
  }
  return true;
}

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float smootherstep(float edge0, float edge1, float value) {
  const float t = clamp01((value - edge0) / (edge1 - edge0));
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

void normalize3(float vector[3]) {
  const float length =
      std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                vector[2] * vector[2]);
  if (length <= 0.000001f) {
    vector[0] = 0.0f;
    vector[1] = 1.0f;
    vector[2] = 0.0f;
    return;
  }

  vector[0] /= length;
  vector[1] /= length;
  vector[2] /= length;
}

sky_uniforms build_sky_uniforms(const server_world_time_state &world_time,
                                const octaryn_client_camera &camera) {
  const float day_fraction = clamp01(world_time.day_fraction);
  const float angle = day_fraction * kPi * 2.0f - kPi * 0.5f;
  float sun_direction[3] = {
      std::cos(angle) * 0.28f,
      std::sin(angle),
      std::cos(angle) * 0.96f,
  };
  normalize3(sun_direction);

  const float day_visibility = smootherstep(-0.10f, 0.25f, sun_direction[1]);
  const float night_visibility = 1.0f - day_visibility;
  const float twilight =
      smootherstep(-0.28f, 0.02f, sun_direction[1]) *
      (1.0f - smootherstep(0.06f, 0.36f, sun_direction[1]));
  sky_uniforms uniforms{};
  uniforms.light_direction_and_sky_visibility[0] = -sun_direction[0];
  uniforms.light_direction_and_sky_visibility[1] = -sun_direction[1];
  uniforms.light_direction_and_sky_visibility[2] = -sun_direction[2];
  uniforms.light_direction_and_sky_visibility[3] =
      std::max(0.08f, day_visibility);
  uniforms.twilight_celestial_cloud_time[0] = twilight;
  uniforms.twilight_celestial_cloud_time[1] = night_visibility;
  uniforms.twilight_celestial_cloud_time[2] = 1.0f;
  uniforms.twilight_celestial_cloud_time[3] =
      static_cast<float>(std::fmod(world_time.total_seconds, 86400.0));
  uniforms.camera_position_and_cloud_height[0] = camera.position[0];
  uniforms.camera_position_and_cloud_height[1] = camera.position[1];
  uniforms.camera_position_and_cloud_height[2] = camera.position[2];
  uniforms.camera_position_and_cloud_height[3] = 192.0f;
  uniforms.celestial_toggles[0] = 1.0f;
  uniforms.celestial_toggles[1] = 1.0f;
  uniforms.celestial_toggles[2] = 1.0f;
  uniforms.celestial_toggles[3] = 0.0f;
  return uniforms;
}

matrix_uniform matrix_from_camera_values(const float values[4][4]) {
  matrix_uniform output{};
  std::memcpy(output.values, values, sizeof(output.values));
  return output;
}

camera_uniforms camera_uniform_from_camera(const octaryn_client_camera &camera) {
  camera_uniforms uniforms{};
  uniforms.position[0] = camera.position[0];
  uniforms.position[1] = camera.position[1];
  uniforms.position[2] = camera.position[2];
  uniforms.position[3] = 1.0f;
  return uniforms;
}

bool draw_shader_world(
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target_texture,
    const BasegameAtlas &atlas, const client_shader_pipelines &pipelines,
    const world_mesh_gpu_buffers &mesh_buffers,
    const world_mesh_upload_frame &mesh_frame,
    const octaryn_client_camera &camera,
    const server_world_time_state &world_time) {
  if (pipelines.sky == nullptr || pipelines.world == nullptr ||
      pipelines.atlas_sampler == nullptr || mesh_buffers.opaque_faces == nullptr) {
    return true;
  }

  SDL_GPUColorTargetInfo target{};
  target.texture = target_texture;
  target.load_op = SDL_GPU_LOADOP_LOAD;
  target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1u, nullptr);
  if (render_pass == nullptr) {
    log_line("live_shader_render_pass=failed");
    return false;
  }

  const matrix_uniform projection = matrix_from_camera_values(camera.projection);
  const matrix_uniform view = matrix_from_camera_values(camera.view);
  const sky_uniforms sky = build_sky_uniforms(world_time, camera);
  const camera_uniforms camera_uniform = camera_uniform_from_camera(camera);

  SDL_PushGPUVertexUniformData(command_buffer, 0u, &projection,
                               sizeof(projection));
  SDL_PushGPUVertexUniformData(command_buffer, 1u, &view, sizeof(view));
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &sky, sizeof(sky));
  SDL_BindGPUGraphicsPipeline(render_pass, pipelines.sky);
  SDL_DrawGPUPrimitives(render_pass, 36u, 1u, 0u, 0u);

  SDL_GPUTextureSamplerBinding atlas_binding{};
  atlas_binding.texture = atlas.color_texture;
  atlas_binding.sampler = pipelines.atlas_sampler;
  SDL_BindGPUFragmentSamplers(render_pass, 0u, &atlas_binding, 1u);
  SDL_GPUBuffer *storage_buffers[2] = {
      mesh_buffers.opaque_faces,
      mesh_buffers.opaque_faces,
  };
  SDL_BindGPUVertexStorageBuffers(render_pass, 0u, storage_buffers, 2u);
  SDL_PushGPUVertexUniformData(command_buffer, 3u, &camera_uniform,
                               sizeof(camera_uniform));
  SDL_BindGPUGraphicsPipeline(render_pass, pipelines.world);

  uint32_t drawn_chunks = 0u;
  uint64_t drawn_faces = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       mesh_frame.chunks) {
    if (chunk.opaque_face_count == 0u) {
      continue;
    }

    chunk_uniforms chunk_uniform{};
    chunk_uniform.chunk_position[0] = chunk.chunk_x * 32;
    chunk_uniform.chunk_position[1] = chunk.chunk_z * 32;
    chunk_uniform.face_offset = static_cast<uint32_t>(chunk.opaque_face_offset);
    chunk_uniform.draw_flags = kDrawFlagUseFaceBuffer;
    SDL_PushGPUVertexUniformData(command_buffer, 2u, &chunk_uniform,
                                 sizeof(chunk_uniform));
    SDL_DrawGPUPrimitives(render_pass, chunk.opaque_face_count * 6u, 1u, 0u,
                          0u);
    ++drawn_chunks;
    drawn_faces += chunk.opaque_face_count;
  }

  SDL_EndGPURenderPass(render_pass);
  if (g_log != nullptr && drawn_faces != 0u) {
    std::fprintf(g_log,
                 "live_sky_pass active=1 source=server_world_time "
                 "day_fraction=%.6f total_seconds=%.3f\n",
                 world_time.day_fraction, world_time.total_seconds);
    std::fprintf(g_log,
                 "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline "
                 "active=1 chunks=%" PRIu32 " opaque_faces=%" PRIu64 "\n",
                 drawn_chunks, drawn_faces);
    std::fflush(g_log);
  }
  return true;
}

void log_live_client_frame(uint64_t frame_index,
                           const client_input_debug_state &input,
                           const client_command_frame_counts &commands,
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
                 " primary=%d secondary=%d command_enqueue_hook=active"
                 " commands_enqueued=%" PRIu32 " set_block=%" PRIu32
                 " place=%" PRIu32 " break=%" PRIu32 "\n",
                 frame_index, (input.flags & kInputPrimaryFlag) != 0u ? 1 : 0,
                 (input.flags & kInputSecondaryFlag) != 0u ? 1 : 0,
                 commands.enqueued, commands.set_block, commands.place_block,
                 commands.break_block);
    std::fprintf(g_log,
                 "live_presentation_frame frame=%" PRIu64
                 " blocks=%zu drained_updates=%" PRIu32 "\n",
                 frame_index, blocks.size(), drained_updates);
    std::fflush(g_log);
  }
}

bool window_output_size(SDL_Window *window, int *width, int *height) {
  if (!SDL_GetWindowSizeInPixels(window, width, height)) {
    log_line("window_output_size=failed");
    return false;
  }

  if (*width <= 0 || *height <= 0) {
    log_line("window_output_size=invalid");
    return false;
  }

  return true;
}

int block_draw_size_for(size_t block_count) {
  return block_count > 1u ? kWorldBlockDrawSize : kBlockDrawSize;
}

void place_camera_over_snapshot(octaryn_client_camera &camera,
                                const std::vector<presentation_block> &blocks) {
  if (blocks.empty()) {
    return;
  }

  int32_t min_x = blocks.front().x;
  int32_t max_x = blocks.front().x;
  int32_t min_y = blocks.front().y;
  int32_t max_y = blocks.front().y;
  int32_t min_z = blocks.front().z;
  int32_t max_z = blocks.front().z;
  for (const presentation_block &block : blocks) {
    min_x = std::min(min_x, block.x);
    max_x = std::max(max_x, block.x);
    min_y = std::min(min_y, block.y);
    max_y = std::max(max_y, block.y);
    min_z = std::min(min_z, block.z);
    max_z = std::max(max_z, block.z);
  }

  camera.position[0] = (static_cast<float>(min_x) + static_cast<float>(max_x)) * 0.5f;
  camera.position[1] = static_cast<float>(min_y) + 2.0f;
  camera.position[2] = (static_cast<float>(min_z) + static_cast<float>(max_z)) * 0.5f;
  octaryn_client_camera_update(&camera);

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "snapshot_camera_origin x=%.3f y=%.3f z=%.3f "
                 "bounds=(%" PRId32 ",%" PRId32 ")-"
                 "(%" PRId32 ",%" PRId32 ")-"
                 "(%" PRId32 ",%" PRId32 ")\n",
                 camera.position[0], camera.position[1], camera.position[2],
                 min_x, max_x, min_y, max_y, min_z, max_z);
    std::fflush(g_log);
  }
}

void log_chunk_view_if_changed(uint64_t frame_index,
                               const octaryn_client_chunk_view &view,
                               octaryn_client_chunk_view &logged_view) {
  if (octaryn_client_chunk_view_equal(&view, &logged_view) != 0 &&
      frame_index % 60u != 0u) {
    return;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_view frame=%" PRIu64
                 " origin=(%d,%d) width=%d radius=%d source=old_arch_window_math "
                 "authority=server\n",
                 frame_index, view.origin_x, view.origin_z, view.width,
                 view.width / 2);
    std::fflush(g_log);
  }

  if (!write_chunk_view_intent(view, frame_index)) {
    return;
  }

  logged_view = view;
}

bool blit_gpu_texture(SDL_GPUCommandBuffer *command_buffer,
                      SDL_GPUTexture *source_texture,
                      SDL_GPUTexture *target_texture, int source_x,
                      int source_y, int source_size, int target_x,
                      int target_y, int target_size, uint32_t target_width,
                      uint32_t target_height) {
  if (target_x + target_size <= 0 || target_y + target_size <= 0 ||
      target_x >= static_cast<int>(target_width) ||
      target_y >= static_cast<int>(target_height)) {
    return true;
  }

  SDL_GPUBlitInfo blit{};
  blit.source.texture = source_texture;
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
                           const octaryn_client_camera &camera,
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

bool draw_blocks(SDL_GPUCommandBuffer *command_buffer,
                 SDL_GPUTexture *target_texture, uint32_t target_width,
                 uint32_t target_height, const BasegameAtlas &atlas,
                 const std::vector<presentation_block> &blocks,
                 const octaryn_client_camera &camera, int &drawn_tiles) {
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
        basegame_atlas_top_layer_for_block(atlas, block.block);
    if (layer < 0) {
      continue;
    }

    int screen_x = 0;
    int screen_y = 0;
    block_screen_position(block, camera, block_draw_size, target_width,
                          target_height, screen_x, screen_y);
    if (!blit_gpu_texture(command_buffer, atlas.color_texture, target_texture,
                          layer * atlas.tile_size, 0, atlas.tile_size,
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

bool draw_material_atlas_probe(SDL_GPUCommandBuffer *command_buffer,
                               SDL_GPUTexture *target_texture,
                               uint32_t target_width, uint32_t target_height,
                               const BasegameAtlas &atlas) {
  if (!read_enabled_flag(kPixelValidationFlag)) {
    return true;
  }
  if (atlas.normal_texture == nullptr || atlas.specular_texture == nullptr ||
      kMaterialAtlasProbeLayer >= atlas.layer_count) {
    log_line("material_atlas_probe=invalid");
    return false;
  }

  const int source_x = kMaterialAtlasProbeLayer * atlas.tile_size;
  if (!blit_gpu_texture(command_buffer, atlas.normal_texture, target_texture,
                        source_x, 0, atlas.tile_size,
                        kMaterialAtlasProbeNormalX, kMaterialAtlasProbeY,
                        kMaterialAtlasProbeSize, target_width,
                        target_height) ||
      !blit_gpu_texture(command_buffer, atlas.specular_texture, target_texture,
                        source_x, 0, atlas.tile_size,
                        kMaterialAtlasProbeSpecularX, kMaterialAtlasProbeY,
                        kMaterialAtlasProbeSize, target_width,
                        target_height)) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log, "material_atlas_tiles_drawn=2\n");
    std::fflush(g_log);
  }
  return true;
}

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

bool present_frame(SDL_GPUDevice *device, SDL_Window *window,
                   const BasegameAtlas &atlas,
                   const std::vector<presentation_block> &blocks,
                   const octaryn_client_camera &camera,
                   const client_shader_pipelines &pipelines,
                   const world_mesh_gpu_buffers &mesh_buffers,
                   const world_mesh_upload_frame &mesh_frame,
                   const server_world_time_state &world_time) {
  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    log_line("gpu_command_buffer=failed");
    return false;
  }

  SDL_GPUTexture *swapchain_texture = nullptr;
  uint32_t target_width = 0u;
  uint32_t target_height = 0u;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          command_buffer, window, &swapchain_texture, &target_width,
          &target_height) ||
      swapchain_texture == nullptr) {
    log_line("gpu_swapchain_acquire=failed");
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!g_gpu_path_logged && g_log != nullptr) {
    std::fprintf(g_log, "gpu_render_path=SDL_GPU\n");
    std::fprintf(g_log, "gpu_swapchain_acquired width=%" PRIu32
                         " height=%" PRIu32 "\n",
                 target_width, target_height);
    std::fflush(g_log);
    g_gpu_path_logged = true;
  }

  if (!clear_gpu_swapchain(command_buffer, swapchain_texture)) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!draw_shader_world(command_buffer, swapchain_texture, atlas, pipelines,
                         mesh_buffers, mesh_frame, camera, world_time)) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  int drawn_tiles = 0;
  if (!draw_blocks(command_buffer, swapchain_texture, target_width,
                   target_height, atlas, blocks, camera, drawn_tiles)) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!draw_material_atlas_probe(command_buffer, swapchain_texture,
                                 target_width, target_height, atlas)) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (read_enabled_flag(kPixelValidationFlag) && g_log != nullptr) {
    const uint8_t sky_red = static_cast<uint8_t>(
        std::clamp(22.0f + world_time.day_fraction * 42.0f, 0.0f, 255.0f));
    const uint8_t sky_green = static_cast<uint8_t>(
        std::clamp(72.0f + world_time.day_fraction * 84.0f, 0.0f, 255.0f));
    const uint8_t sky_blue = static_cast<uint8_t>(
        std::clamp(132.0f + world_time.day_fraction * 68.0f, 0.0f, 255.0f));
    const bool clear_match =
        sky_red == kClearRed && sky_green == kClearGreen &&
        sky_blue == kClearBlue;
    std::fprintf(g_log,
                 "live_sky_pixel active=%d source=sky_uniform_sample x=%" PRIu32
                 " y=%" PRIu32 " rgba=(%u,%u,%u,%u) clear_match=%d\n",
                 clear_match ? 0 : 1, target_width > 8u ? target_width - 8u : 0u,
                 target_height > 8u ? 8u : 0u, static_cast<unsigned>(sky_red),
                 static_cast<unsigned>(sky_green),
                 static_cast<unsigned>(sky_blue), static_cast<unsigned>(kClearAlpha),
                 clear_match ? 1 : 0);
    std::fflush(g_log);
  }

  if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
    log_line("gpu_submit=failed");
    return false;
  }

  if (drawn_tiles != 0 && g_log != nullptr) {
    std::fprintf(g_log, "gpu_atlas_blits_drawn=%d\n", drawn_tiles);
    std::fflush(g_log);
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

  SDL_GPUDevice *gpu_device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
  if (gpu_device == nullptr) {
    log_line("gpu_device_create=failed");
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
  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
    log_line("gpu_window_claim=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 7;
  }
  log_line("gpu_device_create=0");
  log_line("gpu_window_claim=0");

  BasegameAtlas atlas{};
  if (!load_basegame_module_descriptor() ||
      !load_basegame_atlas(gpu_device, g_log, atlas)) {
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
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
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
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
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      if (g_log != nullptr) {
        std::fclose(g_log);
      }
      return 8;
    }
  }

  std::vector<presentation_block> world_snapshot_blocks;
  std::vector<presentation_block> world_surface_blocks;
  server_world_time_state world_time{};
  if (!load_world_snapshot_blocks(world_snapshot_blocks, world_surface_blocks,
                                  world_time)) {
    octaryn_client_shutdown();
    destroy_basegame_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
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
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      if (g_log != nullptr) {
        std::fclose(g_log);
      }
      return 10;
    }
  }

  client_shader_pipelines shader_pipelines{};
  if (!initialize_shader_pipelines(gpu_device, window, shader_pipelines)) {
    octaryn_client_shutdown();
    destroy_basegame_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 11;
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  octaryn_client_fly_player_controller player{};
  octaryn_client_fly_player_controller_init(&player);
  place_camera_over_snapshot(player.camera, world_surface_blocks);
  octaryn_client_camera_update(&player.camera);
  std::vector<presentation_block> presentation_blocks;
  world_mesh_upload_scratch mesh_upload_scratch{
      std::vector<octaryn_client_chunk_mesh_upload_record>(
          kMaxChunkMeshUploadsPerFrame),
      std::vector<uint64_t>(kMaxPackedOpaqueFacesPerFrame),
      std::vector<uint64_t>(kMaxPackedTransparentFacesPerFrame),
      std::vector<uint32_t>(kMaxPackedSpriteVerticesPerFrame),
  };
  world_mesh_gpu_buffers mesh_buffers{};
  world_mesh_upload_frame visible_world_mesh_frame{};
  client_key_state keys{};
  octaryn_client_chunk_view logged_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
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
        if (event.key.scancode >= 0 &&
            event.key.scancode < static_cast<SDL_Scancode>(keys.size())) {
          keys[static_cast<size_t>(event.key.scancode)] =
              event.type == SDL_EVENT_KEY_DOWN;
        }
        if (g_log != nullptr) {
          std::fprintf(g_log,
                       "live_input_event type=%" PRIu32 " scancode=%d "
                       "repeat=%d down=%d\n",
                       static_cast<uint32_t>(event.type),
                       static_cast<int>(event.key.scancode),
                       event.key.repeat ? 1 : 0,
                       event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
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
    client_input_debug_state input =
        read_client_input(window, pointer_motion, keys);
    apply_input_probe(input, frame.timing.frame_index);
    if (!update_client_player_controller(window, player, input,
                                         frame.timing.delta_seconds)) {
      result = -4;
      running = false;
      break;
    }
    const octaryn_client_camera &camera = player.camera;
    apply_input_to_frame(frame, input, camera);
    const octaryn_client_chunk_view chunk_view =
        octaryn_client_chunk_view_for_camera(camera.position[0],
                                            camera.position[2], 16);
    log_chunk_view_if_changed(frame.timing.frame_index, chunk_view,
                              logged_chunk_view);
    reset_command_frame_counts();
    if (!write_player_input_intent(frame)) {
      result = -7;
      running = false;
      break;
    }
    if (!write_block_interaction_intent(frame, input, camera,
                                        world_snapshot_blocks)) {
      result = -8;
      running = false;
      break;
    }
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
    world_mesh_upload_frame mesh_upload_frame{};
    if (!drain_chunk_mesh_uploads(frame.timing.frame_index, mesh_upload_scratch,
                                  mesh_upload_frame)) {
      result = -5;
      running = false;
      break;
    }
    if (!upload_world_mesh_frame(gpu_device, mesh_upload_frame, mesh_buffers,
                                 frame.timing.frame_index)) {
      result = -6;
      running = false;
      break;
    }
    if (!mesh_upload_frame.chunks.empty()) {
      visible_world_mesh_frame = mesh_upload_frame;
    }
    log_live_client_frame(frame.timing.frame_index, input,
                          g_command_frame_counts, camera,
                          drained_updates, presentation_blocks);

    if (!present_frame(gpu_device, window, atlas, presentation_blocks, camera,
                       shader_pipelines, mesh_buffers, visible_world_mesh_frame,
                       world_time)) {
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
  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  release_shader_pipelines(gpu_device, shader_pipelines);
  destroy_basegame_atlas(atlas);
  SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
  SDL_DestroyGPUDevice(gpu_device);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (g_log != nullptr) {
    std::fclose(g_log);
  }

  return result == 0 ? 0 : 6;
}
