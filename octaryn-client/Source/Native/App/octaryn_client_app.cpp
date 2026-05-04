#include "octaryn_client_app_environment.h"
#include "octaryn_client_app_file_io.h"
#include "octaryn_client_app_host_commands.h"
#include "octaryn_client_app_input.h"
#include "octaryn_client_app_json_files.h"
#include "octaryn_client_app_log.h"
#include "octaryn_client_app_window.h"
#include "octaryn_client_app_world_intents.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_chunk_view.h"
#include "octaryn_client_frame_profile.h"
#include "octaryn_client_function_profile.h"
#include "octaryn_client_fly_player_controller.h"
#include "octaryn_client_host_exports.h"
#include "octaryn_client_render_distance.h"
#include "octaryn_client_runtime_controls.h"
#include "octaryn_client_runtime_settings.h"
#include "octaryn_client_shader_creation.h"
#include "octaryn_client_swapchain.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_singleplayer_server_session.h"
#include "octaryn_native_crash_diagnostics.h"

#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using octaryn::client::rendering::client_block_atlas_top_layer_for_block;
using octaryn::client::rendering::client_block_atlas_default_placeable_block;
using octaryn::client::rendering::client_block_atlas_scroll_placeable_block;
using octaryn::client::rendering::ClientBlockAtlas;
using octaryn::client::rendering::ComputeShaderMetadata;
using octaryn::client::rendering::create_compute_pipeline;
using octaryn::client::rendering::create_graphics_shader;
using octaryn::client::rendering::destroy_client_block_atlas;
using octaryn::client::rendering::GraphicsShaderMetadata;
using octaryn::client::rendering::load_client_block_atlas;
using octaryn_client_app::build_client_bundle_path;
using octaryn_client_app::client_block_interaction_command_file;
using octaryn_client_app::client_block_interaction_intent_file;
using octaryn_client_app::client_chunk_view_intent_file;
using octaryn_client_app::client_player_input_intent_file;
using octaryn_client_app::client_world_time_intent_file;
using octaryn_client_app::close_log;
using octaryn_client_app::compute_shader_metadata_file;
using octaryn_client_app::g_log;
using octaryn_client_app::graphics_shader_metadata_file;
using octaryn_client_app::client_command_frame_counts;
using octaryn_client_app::client_input_debug_state;
using octaryn_client_app::client_key_state;
using octaryn_client_app::client_world_time_controls;
using octaryn_client_app::command_frame_counts;
using octaryn_client_app::create_frame;
using octaryn_client_app::enqueue_command;
using octaryn_client_app::frame_delta_seconds;
using octaryn_client_app::apply_input_probe;
using octaryn_client_app::apply_input_to_frame;
using octaryn_client_app::kHostCommandClientInteractionFlag;
using octaryn_client_app::kHostCommandCriticalFlag;
using octaryn_client_app::kInputPrimaryFlag;
using octaryn_client_app::kInputProbeFlag;
using octaryn_client_app::kInputSecondaryFlag;
using octaryn_client_app::kInputSprintFlag;
using octaryn_client_app::log_client_tick_input_frame;
using octaryn_client_app::log_line;
using octaryn_client_app::log_result;
using octaryn_client_app::open_log;
using octaryn_client_app::pointer_click_debug_state;
using octaryn_client_app::pointer_motion_debug_state;
using octaryn_client_app::prepare_singleplayer_server_session;
using octaryn_client_app::read_binary_file;
using octaryn_client_app::read_client_input;
using octaryn_client_app::read_enabled_flag;
using octaryn_client_app::read_exit_after_frames;
using octaryn_client_app::read_text_file;
using octaryn_client_app::server_chunk_stream_file;
using octaryn_client_app::singleplayer_server_session;
using octaryn_client_app::start_singleplayer_server;
using octaryn_client_app::stop_singleplayer_server;
using octaryn_client_app::reset_command_frame_counts;
using octaryn_client_app::update_client_player_controller;
using octaryn_client_app::window_output_size;
using octaryn_client_app::write_text_file_atomic;
using octaryn_client_app::write_chunk_view_intent;
using octaryn_client_app::write_player_input_intent;
using octaryn_client_app::write_world_time_intent;
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
constexpr uint32_t kMaxChunkMeshUploadsPerFrame = 4096u;
constexpr uint32_t kMaxPackedOpaqueFacesPerFrame = 8388608u;
constexpr uint32_t kMaxPackedTransparentFacesPerFrame = 1048576u;
constexpr uint32_t kMaxPackedSpriteVerticesPerFrame = 4194304u;
constexpr const char *kPixelValidationFlag =
    "OCTARYN_CLIENT_APP_VALIDATE_PIXELS";
constexpr const char *kDisableGameModulesFlag =
    "OCTARYN_CLIENT_DISABLE_GAME_MODULES";
constexpr std::array<double, 7> kWorldTimeSpeedMultipliers{
    0.0, 1.0, 60.0, 300.0, 1200.0, 6000.0, 24000.0};
constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};
constexpr glz::opts kJsonWriteOptions{.prettify = true};
constexpr uint32_t kDrawFlagUseFaceBuffer = 1u << 1u;
constexpr uint16_t kDefaultInteractionPlaceBlock = 29u;
constexpr float kBlockInteractionReachBlocks = 6.0f;
constexpr float kBlockInteractionRayStepBlocks = 0.05f;
constexpr int32_t kNativeEmptyWorldChunkSize = 32;
constexpr int32_t kNativeEmptyWorldMinY = -256;
constexpr int32_t kNativeEmptyWorldMaxYExclusive = 256;
constexpr int32_t kNativeEmptyWorldMinChunkY = -8;
constexpr int32_t kNativeEmptyWorldChunkY = -1;
constexpr int32_t kNativeEmptyWorldLocalY = 31;
constexpr int32_t kNativeEmptyWorldAtlasTileSize = 16;
constexpr uint32_t kClientChunkMeshUploadRecordVersion = 1u;
constexpr uint32_t kClientChunkMeshUploadRecordSize = 96u;
constexpr uint32_t kClientChunkMeshClearTransparentFacesFlag = 1u << 1u;
constexpr uint32_t kClientChunkMeshClearSpriteVerticesFlag = 1u << 2u;
constexpr uint32_t kClientChunkMeshClearFluidBlocksFlag = 1u << 3u;
constexpr uint32_t kPackedFaceXOffset = 0u;
constexpr uint32_t kPackedFaceYOffset = 5u;
constexpr uint32_t kPackedFaceZOffset = 13u;
constexpr uint32_t kPackedFaceDirectionOffset = 18u;
constexpr uint32_t kPackedFaceSpanUOffset = 21u;
constexpr uint32_t kPackedFaceSpanVOffset = 29u;
constexpr uint32_t kPackedFaceAtlasLayerOffset = 37u;
constexpr uint32_t kPackedFaceOcclusionOffset = 43u;
constexpr uint32_t kPackedFaceChunkSlotOffset = 44u;
constexpr uint32_t kPackedFaceWaterLevelOffset = 57u;
constexpr uint32_t kPackedFaceWaterFlagOffset = 60u;
constexpr uint32_t kPackedFaceWaterBaseHeightOffset = 61u;
constexpr uint64_t kPackedFaceUnsetChunkSlot = 0x1fffu;

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

struct block_selection_state {
  uint16_t selected_block = kDefaultInteractionPlaceBlock;
  uint64_t change_count = 0u;
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

struct gpu_pixel_readback {
  SDL_GPUTransferBuffer *transfer = nullptr;
  Uint32 row_pitch = 0u;
  Uint32 texel_size = 0u;
  Uint32 x = 0u;
  Uint32 y = 0u;
};

struct compiled_graphics_shader {
  GraphicsShaderMetadata metadata{};
  std::vector<uint8_t> code;
};

struct compiled_compute_shader {
  ComputeShaderMetadata metadata{};
  std::vector<uint8_t> code;
};

struct client_shader_pipelines {
  SDL_GPUGraphicsPipeline *sky = nullptr;
  SDL_GPUGraphicsPipeline *world = nullptr;
  SDL_GPUGraphicsPipeline *opaque_sprite = nullptr;
  SDL_GPUGraphicsPipeline *present = nullptr;
  SDL_GPUComputePipeline *composite = nullptr;
  SDL_GPUComputePipeline *ui = nullptr;
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
  int32_t chunk_position[4]{};
  uint32_t face_offset = 0u;
  uint32_t draw_flags = 0u;
};

struct camera_uniforms {
  float position[4]{};
};

struct opaque_fragment_uniforms {
  float skylight_floor = 0.0f;
  float cloud_time_seconds = 0.0f;
  float sky_visibility = 1.0f;
  float twilight_strength = 0.0f;
  float celestial_visibility = 1.0f;
  uint32_t material_flags = 0u;
  uint32_t pad0 = 0u;
  uint32_t pad1 = 0u;
  float camera_position[4]{};
};

struct composite_uniforms {
  float sky_visibility_and_ambient_strength[4]{};
};

struct hidden_block_uniforms {
  uint32_t hidden_block_count = 0u;
  int32_t hidden_pad[3]{};
  int32_t hidden_blocks[32][4]{};
};

struct block_highlight_uniforms {
  uint32_t highlight_block_enabled = 0u;
  int32_t highlight_pad[3]{};
  int32_t highlight_block[4]{};
};

struct ui_viewport_uniforms {
  int32_t viewport[2]{};
  int32_t offset[2]{};
};

struct ui_uniforms {
  uint32_t index = 0u;
  uint32_t debug_enabled = 0u;
  uint32_t fps_tenths = 0u;
  uint32_t frame_time_hundredths = 0u;
  uint32_t profile_frame_time_hundredths = 0u;
  uint32_t fps_average_tenths = 0u;
  uint32_t fps_low_1_tenths = 0u;
  uint32_t fps_low_0_1_tenths = 0u;
  uint32_t fps_low_x5_tenths = 0u;
  uint32_t fps_low_x10_tenths = 0u;
  uint32_t fps_worst_tenths = 0u;
  uint32_t warmup_complete = 1u;
  uint32_t sample_count = 0u;
  uint32_t ms_low_1_hundredths = 0u;
  uint32_t ms_low_0_1_hundredths = 0u;
  uint32_t ms_low_x5_hundredths = 0u;
  uint32_t ms_low_x10_hundredths = 0u;
  uint32_t ms_worst_hundredths = 0u;
  uint32_t warmup_elapsed_hundredths = 0u;
  uint32_t warmup_total_hundredths = 0u;
  uint32_t sim_time_hundredths = 0u;
  uint32_t misc_time_hundredths = 0u;
  uint32_t world_time_hundredths = 0u;
  uint32_t render_time_hundredths = 0u;
  uint32_t render_setup_hundredths = 0u;
  uint32_t render_other_time_hundredths = 0u;
  uint32_t gbuffer_time_hundredths = 0u;
  uint32_t gbuffer_sky_hundredths = 0u;
  uint32_t gbuffer_opaque_hundredths = 0u;
  uint32_t gbuffer_sprite_hundredths = 0u;
  uint32_t post_time_hundredths = 0u;
  uint32_t composite_time_hundredths = 0u;
  uint32_t depth_time_hundredths = 0u;
  uint32_t forward_time_hundredths = 0u;
  uint32_t ui_time_hundredths = 0u;
  uint32_t imgui_time_hundredths = 0u;
  uint32_t swapchain_blit_hundredths = 0u;
  uint32_t render_submit_hundredths = 0u;
  uint32_t untracked_time_hundredths = 0u;
  uint32_t cpu_ram_hundredths_gib = 0u;
  uint32_t gpu_vram_hundredths_gib = 0u;
  uint32_t cpu_load_hundredths = 0u;
  uint32_t gpu_load_hundredths = 0u;
  uint32_t menu_enabled = 0u;
  uint32_t menu_row = 0u;
  uint32_t menu_display = 0u;
  uint32_t menu_mode_width = 0u;
  uint32_t menu_mode_height = 0u;
  uint32_t menu_fullscreen = 0u;
  uint32_t menu_present_mode = 0u;
  uint32_t menu_fog = 0u;
  uint32_t menu_render_distance = 0u;
  uint32_t menu_clouds = 0u;
  uint32_t menu_sky_gradient = 0u;
  uint32_t menu_stars = 0u;
  uint32_t menu_sun = 0u;
  uint32_t menu_moon = 0u;
  uint32_t menu_pom = 0u;
  uint32_t menu_pbr = 0u;
};

bool g_gpu_path_logged;
bool g_sky_path_logged;
bool g_composite_path_logged;
bool g_present_path_logged;

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

bool load_compute_shader_asset(const char *shader_name,
                               compiled_compute_shader &shader) {
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
                                "client_compute_shader_metadata_path=failed") ||
      !build_client_bundle_path(spirv_path, sizeof(spirv_path),
                                relative_spirv.c_str(),
                                "client_compute_shader_spirv_path=failed")) {
    return false;
  }

  std::string metadata_payload;
  if (!read_text_file(metadata_path, "client_compute_shader_metadata=open_failed",
                      metadata_payload) ||
      !read_binary_file(spirv_path, "client_compute_shader_spirv=open_failed",
                        shader.code)) {
    return false;
  }

  compute_shader_metadata_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, metadata_payload);
  if (error) {
    log_line("client_compute_shader_metadata=parse_failed");
    return false;
  }

  shader.metadata.samplers = file.samplers;
  shader.metadata.readonlyStorageTextures = file.readonly_storage_textures;
  shader.metadata.readonlyStorageBuffers = file.readonly_storage_buffers;
  shader.metadata.readwriteStorageTextures = file.readwrite_storage_textures;
  shader.metadata.readwriteStorageBuffers = file.readwrite_storage_buffers;
  shader.metadata.uniformBuffers = file.uniform_buffers;
  shader.metadata.threadcountX = file.threadcount_x;
  shader.metadata.threadcountY = file.threadcount_y;
  shader.metadata.threadcountZ = file.threadcount_z;
  return !shader.code.empty();
}

SDL_GPUComputePipeline *create_compute_shader_pipeline(
    SDL_GPUDevice *device, const char *shader_name) {
  compiled_compute_shader shader{};
  if (!load_compute_shader_asset(shader_name, shader)) {
    return nullptr;
  }

  SDL_GPUComputePipeline *pipeline = create_compute_pipeline(
      device, shader.metadata, shader.code.data(), shader.code.size(), "main",
      SDL_GPU_SHADERFORMAT_SPIRV);
  if (pipeline == nullptr) {
    log_line("client_compute_pipeline=create_failed");
  }
  return pipeline;
}

SDL_GPUGraphicsPipeline *create_swapchain_pipeline(
    SDL_GPUDevice *device, SDL_GPUTextureFormat color_format,
    SDL_GPUTextureFormat depth_format, bool enable_depth,
    SDL_GPUCullMode cull_mode,
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
  SDL_GPUColorTargetDescription color_targets[4]{};
  color_targets[0].format = color_format;
  color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  color_targets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  color_targets[3].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vertex;
  info.fragment_shader = fragment;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode = cull_mode;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  info.target_info.color_target_descriptions =
      enable_depth ? color_targets : &color_target;
  info.target_info.num_color_targets = enable_depth ? 4u : 1u;
  if (enable_depth) {
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  }
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
  constexpr SDL_GPUTextureFormat color_format =
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  constexpr SDL_GPUTextureFormat depth_format =
      SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  pipelines.sky = create_swapchain_pipeline(device, color_format,
                                            depth_format, false,
                                            SDL_GPU_CULLMODE_NONE,
                                            "sky.vert", "sky.frag");
  pipelines.world = create_swapchain_pipeline(device, color_format,
                                              depth_format, true,
                                              SDL_GPU_CULLMODE_BACK,
                                              "opaque_packed.vert",
                                              "opaque.frag");
  pipelines.opaque_sprite =
      create_swapchain_pipeline(device, color_format, depth_format, true,
                                SDL_GPU_CULLMODE_NONE, "sprite_packed.vert",
                                "opaque.frag");
  pipelines.present = create_swapchain_pipeline(device, swapchain_format,
                                                depth_format, false,
                                                SDL_GPU_CULLMODE_NONE,
                                                "present.vert", "present.frag");
  pipelines.composite = create_compute_shader_pipeline(device, "composite.comp");
  pipelines.ui = create_compute_shader_pipeline(device, "ui.comp");

  SDL_GPUSamplerCreateInfo sampler_info{};
  sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  pipelines.atlas_sampler = SDL_CreateGPUSampler(device, &sampler_info);

  if (pipelines.sky == nullptr || pipelines.world == nullptr ||
      pipelines.opaque_sprite == nullptr || pipelines.present == nullptr ||
      pipelines.composite == nullptr || pipelines.ui == nullptr ||
      pipelines.atlas_sampler == nullptr) {
    log_line("live_shader_pipeline active=0 reason=create_failed");
    return false;
  }

  log_line("live_shader_pipeline active=1 sky=1 world=1 opaque_sprite=1 present=1 composite=1 ui=1 block_highlight=texture source=compiled_spirv");
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
  if (pipelines.opaque_sprite != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.opaque_sprite);
    pipelines.opaque_sprite = nullptr;
  }
  if (pipelines.present != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.present);
    pipelines.present = nullptr;
  }
  if (pipelines.composite != nullptr) {
    SDL_ReleaseGPUComputePipeline(device, pipelines.composite);
    pipelines.composite = nullptr;
  }
  if (pipelines.ui != nullptr) {
    SDL_ReleaseGPUComputePipeline(device, pipelines.ui);
    pipelines.ui = nullptr;
  }
  if (pipelines.sky != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.sky);
    pipelines.sky = nullptr;
  }
}

bool load_bundled_game_module_descriptor() {
  char directory_buffer[4096] = {};
  if (!build_client_bundle_path(directory_buffer, sizeof(directory_buffer),
                                "Data/Module",
                                "game_module_descriptor_path=failed")) {
    return false;
  }

  const std::filesystem::path directory(directory_buffer);
  if (!std::filesystem::exists(directory)) {
    log_line("game_module_descriptor_path=failed");
    return false;
  }

  std::vector<std::filesystem::path> manifests;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() &&
        entry.path().filename().string().ends_with(".module.json")) {
      manifests.push_back(entry.path());
    }
  }

  if (manifests.empty()) {
    log_line("game_module_descriptor_path=failed");
    return false;
  }

  std::sort(manifests.begin(), manifests.end());
  std::string payload;
  if (!read_text_file(manifests.front().string().c_str(),
                      "game_module_descriptor=open_failed",
                      payload)) {
    return false;
  }

  if (payload.find("\"ModuleId\"") == std::string::npos) {
    log_line("game_module_descriptor=invalid");
    return false;
  }

  log_line("game_module_descriptor=loaded");
  return true;
}

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

bool load_native_empty_world_atlas(SDL_GPUDevice *device,
                                   ClientBlockAtlas &atlas) {
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

uint64_t pack_column_key(int32_t x, int32_t z) {
  return static_cast<uint32_t>(x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(z)) << 32u);
}

bool is_spawn_column_block(const world_block_record &block) {
  return block.block != 0u && block.x >= kWorldSnapshotMinX &&
         block.x < kWorldSnapshotMaxXExclusive &&
         block.z >= kWorldSnapshotMinZ && block.z < kWorldSnapshotMaxZExclusive;
}

bool apply_top_blocks_from_records(
    const std::vector<world_block_record> &records, bool spawn_only,
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

bool load_world_snapshot_blocks(
    std::vector<presentation_block> &snapshot_blocks,
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
    const auto stream_error =
        glz::read<kJsonReadOptions>(stream, stream_payload);
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

bool load_world_blocks_from_path(
    const std::filesystem::path &path,
    std::vector<presentation_block> &snapshot_blocks,
    std::vector<presentation_block> &surface_blocks) {
  std::string payload;
  if (!read_text_file(path.string().c_str(), "world_blocks_file=open_failed",
                      payload)) {
    return false;
  }

  world_block_file file{};
  const auto error = glz::read<kJsonReadOptions>(file, payload);
  if (error || file.version != 1) {
    log_line("world_blocks_file=parse_failed");
    return false;
  }

  apply_blocks_from_records(file.blocks, true, snapshot_blocks);
  apply_top_blocks_from_records(file.blocks, true, surface_blocks);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "server_world_blocks_loaded=%zu surface_blocks=%zu\n",
                 file.blocks.size(), surface_blocks.size());
    std::fflush(g_log);
  }
  return !snapshot_blocks.empty();
}

bool load_server_chunk_stream_file(server_chunk_stream_file &stream,
                                   server_world_time_state &world_time,
                                   bool missing_is_waiting) {
  const char *stream_path = std::getenv("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH");
  if (stream_path == nullptr || stream_path[0] == '\0') {
    return false;
  }

  if (!std::filesystem::exists(stream_path)) {
    if (missing_is_waiting) {
      log_line("server_chunk_stream_file=waiting");
      return true;
    }

    log_line("server_chunk_stream_file=open_failed");
    return false;
  }

  std::string stream_payload;
  if (!read_text_file(stream_path, "server_chunk_stream_file=open_failed",
                      stream_payload)) {
    return false;
  }

  server_chunk_stream_file loaded{};
  const auto stream_error =
      glz::read<kJsonReadOptions>(loaded, stream_payload);
  if (stream_error) {
    if (missing_is_waiting) {
      log_line("server_chunk_stream_file=waiting reason=partial_write");
      return true;
    }

    log_line("server_chunk_stream_file=parse_failed");
    return false;
  }

  if (loaded.version != 1 ||
      loaded.source != "server_process_chunk_stream") {
    log_line("server_chunk_stream_file=unsupported_version");
    return false;
  }

  stream = std::move(loaded);
  world_time.active = true;
  world_time.day_index = stream.worldTimeDayIndex;
  world_time.second_of_day = stream.worldTimeSecondOfDay;
  world_time.total_seconds = stream.worldTimeTotalSeconds;
  world_time.day_fraction =
      std::clamp(stream.worldTimeDayFraction, 0.0f, 1.0f);
  if (g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_chunk_streaming active=1 source=server_background epoch=%" PRIu64
        " center=(%d,%d) radius=%" PRIu32
        " columns=%zu blocks=%zu world_time_day_fraction=%.6f\n",
        stream.epoch, stream.centerChunkX, stream.centerChunkZ, stream.radius,
        stream.columns.size(), stream.blocks.size(), world_time.day_fraction);
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

bool has_block_override(const block_lookup &lookup, const block_position_key &key,
                        uint16_t &block) {
  const auto iterator = lookup.find(key);
  if (iterator == lookup.end()) {
    block = 0u;
    return false;
  }

  block = iterator->second;
  return true;
}

uint16_t native_empty_generated_block(const block_position_key &key) {
  return key.y >= kNativeEmptyWorldMinY &&
                 key.y < 0 &&
                 key.y < kNativeEmptyWorldMaxYExclusive
             ? 1u
             : 0u;
}

uint16_t native_empty_effective_block(const block_lookup &overrides,
                                      const block_position_key &key) {
  uint16_t block = 0u;
  return has_block_override(overrides, key, block)
             ? block
             : native_empty_generated_block(key);
}

block_position_key block_position_at(float x, float y, float z) {
  return block_position_key{
      static_cast<int32_t>(std::floor(x)),
      static_cast<int32_t>(std::floor(y)),
      static_cast<int32_t>(std::floor(z)),
  };
}

client_block_raycast_hit
raycast_block_interaction(const octaryn_client_camera &camera,
                          const block_lookup &lookup) {
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
    const block_position_key current =
        block_position_at(camera.position[0] + direction_x * distance,
                          camera.position[1] + direction_y * distance,
                          camera.position[2] + direction_z * distance);
    const uint16_t block = find_block(lookup, current);
    if (block != 0u) {
      return client_block_raycast_hit{
          true,
          current,
          previous == current
              ? block_position_key{current.x, current.y + 1, current.z}
              : previous,
          block,
      };
    }

    previous = current;
  }

  return {};
}

client_block_raycast_hit raycast_native_empty_world_interaction(
    const octaryn_client_camera &camera, const block_lookup &overrides) {
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
    const block_position_key current =
        block_position_at(camera.position[0] + direction_x * distance,
                          camera.position[1] + direction_y * distance,
                          camera.position[2] + direction_z * distance);
    const uint16_t block = native_empty_effective_block(overrides, current);
    if (block != 0u) {
      const uint16_t previous_block =
          native_empty_effective_block(overrides, previous);
      return client_block_raycast_hit{
          true,
          current,
          previous_block == 0u
              ? previous
              : block_position_key{current.x, current.y + 1, current.z},
          block,
      };
    }

    previous = current;
  }

  return {};
}

client_block_interaction_command_file make_block_interaction_command(
    uint64_t request_id, const block_position_key &edit, uint16_t block,
    const octaryn_client_camera &camera, const block_position_key &hit) {
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

void apply_local_block_record(std::vector<presentation_block> &blocks,
                              const presentation_block &update) {
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

bool apply_client_block_interaction_edit(
    const client_block_interaction_command_file &command_file,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    uint64_t tick_id, bool preserve_air_edits) {
  const presentation_block update{
      command_file.editX,
      command_file.editY,
      command_file.editZ,
      command_file.block,
  };
  apply_local_block_record(world_blocks, update);

  const block_position_key key{update.x, update.y, update.z};
  if (update.block == 0u && !preserve_air_edits) {
    lookup.erase(key);
  } else {
    lookup[key] = update.block;
  }

  octaryn_replication_change change{};
  change.version = 1u;
  change.size = OCTARYN_REPLICATION_CHANGE_SIZE;
  change.change_kind = 1u;
  change.replication_id = tick_id;
  change.payload0 = pack_signed_pair(update.x, update.y);
  change.payload1 = pack_block(update.z, update.block);

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = 1u;
  snapshot.tick_id = tick_id;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&change));

  const int result = octaryn_client_apply_server_snapshot(&snapshot);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_client_block_edit_apply result=%d edit=(%d,%d,%d,%u) "
                 "tick=%" PRIu64 "\n",
                 result, update.x, update.y, update.z,
                 static_cast<unsigned>(update.block), tick_id);
    std::fflush(g_log);
  }
  return result == 0;
}

bool write_block_interaction_intent(
    const octaryn_host_frame_snapshot &frame,
    const client_input_debug_state &input, const octaryn_client_camera &camera,
    const client_block_raycast_hit &hit, uint16_t selected_place_block,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    bool preserve_air_edits) {
  const bool primary = (input.flags & kInputPrimaryFlag) != 0u;
  const bool secondary = (input.flags & kInputSecondaryFlag) != 0u;
  if (!primary && !secondary) {
    return true;
  }

  if (!hit.has_hit) {
    log_line("live_block_interaction_intent active=0 reason=raycast_miss");
    return true;
  }

  client_block_interaction_intent_file intent{};
  intent.frameIndex = frame.timing.frame_index;
  const uint64_t request_base = frame.timing.frame_index * 2u;
  if (secondary) {
    intent.commands.push_back(make_block_interaction_command(
        request_base + 1u, hit.adjacent, selected_place_block, camera,
        hit.hit));
  }
  if (primary) {
    intent.commands.push_back(make_block_interaction_command(
        request_base + 2u, hit.hit, 0u, camera, hit.hit));
  }

  for (const client_block_interaction_command_file &command_file :
       intent.commands) {
    octaryn_host_command command = make_logged_interaction_command(command_file);
    enqueue_command(&command);
    if (!apply_client_block_interaction_edit(command_file, world_blocks, lookup,
                                             frame.timing.frame_index + 2u,
                                             preserve_air_edits)) {
      return false;
    }
  }

  const char *path =
      std::getenv("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    log_line("live_block_interaction_intent_write=skipped reason=no_path");
    return true;
  }

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_block_interaction_intent_write=encode_failed");
    return false;
  }

  if (!write_text_file_atomic(std::filesystem::path(path), output,
                              "live_block_interaction_intent_write=failed")) {
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

uint64_t pack_native_empty_face_field(uint64_t packed, uint64_t value,
                                      uint32_t offset, uint64_t mask) {
  return packed | ((value & mask) << offset);
}

uint64_t pack_native_empty_block_face(uint32_t x, uint32_t y, uint32_t z,
                                      uint32_t direction, uint32_t span_u,
                                      uint32_t span_v) {
  uint64_t packed = 0u;
  packed = pack_native_empty_face_field(packed, x, kPackedFaceXOffset, 0x1fu);
  packed = pack_native_empty_face_field(packed, y, kPackedFaceYOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, z, kPackedFaceZOffset, 0x1fu);
  packed = pack_native_empty_face_field(packed, direction,
                                        kPackedFaceDirectionOffset, 0x7u);
  packed = pack_native_empty_face_field(packed, span_u - 1u,
                                        kPackedFaceSpanUOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, span_v - 1u,
                                        kPackedFaceSpanVOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceAtlasLayerOffset, 0x3fu);
  packed =
      pack_native_empty_face_field(packed, 1u, kPackedFaceOcclusionOffset, 0x1u);
  packed = pack_native_empty_face_field(
      packed, kPackedFaceUnsetChunkSlot, kPackedFaceChunkSlotOffset, 0x1fffu);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceWaterLevelOffset, 0x7u);
  packed =
      pack_native_empty_face_field(packed, 0u, kPackedFaceWaterFlagOffset, 0x1u);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceWaterBaseHeightOffset, 0x7u);
  return packed;
}

bool same_chunk_view(const octaryn_client_chunk_view &left,
                     const octaryn_client_chunk_view &right) {
  return left.origin_x == right.origin_x && left.origin_z == right.origin_z &&
         left.width == right.width;
}

octaryn_client_chunk_view
chunk_view_from_server_stream(const server_chunk_stream_file &stream) {
  octaryn_client_chunk_view view{};
  view.origin_x = stream.centerChunkX - static_cast<int32_t>(stream.radius);
  view.origin_z = stream.centerChunkZ - static_cast<int32_t>(stream.radius);
  view.width = static_cast<int32_t>(stream.radius * 2u + 1u);
  return view;
}

uint64_t hash_world_block_records(const std::vector<world_block_record> &records) {
  std::vector<world_block_record> ordered = records;
  std::sort(ordered.begin(), ordered.end(),
            [](const world_block_record &left,
               const world_block_record &right) {
              if (left.x != right.x) {
                return left.x < right.x;
              }
              if (left.y != right.y) {
                return left.y < right.y;
              }
              if (left.z != right.z) {
                return left.z < right.z;
              }
              return left.block < right.block;
            });

  uint64_t hash = 1469598103934665603ull;
  auto append = [&hash](uint64_t value) {
    for (uint32_t byte = 0u; byte < 8u; ++byte) {
      hash ^= (value >> (byte * 8u)) & 0xffu;
      hash *= 1099511628211ull;
    }
  };

  append(static_cast<uint64_t>(ordered.size()));
  for (const world_block_record &record : ordered) {
    append(static_cast<uint32_t>(record.x));
    append(static_cast<uint32_t>(record.y));
    append(static_cast<uint32_t>(record.z));
    append(record.block);
  }
  return hash;
}

int32_t floor_div_int32(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

int32_t floor_mod_int32(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}

void append_native_empty_chunk_faces(world_mesh_upload_frame &mesh_frame,
                                     int32_t chunk_x, int32_t chunk_y,
                                     int32_t chunk_z,
                                     const std::vector<uint64_t> &faces) {
  if (faces.empty()) {
    return;
  }

  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.version = kClientChunkMeshUploadRecordVersion;
  chunk.size = kClientChunkMeshUploadRecordSize;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.chunk_z = chunk_z;
  chunk.flags = kClientChunkMeshClearTransparentFacesFlag |
                kClientChunkMeshClearSpriteVerticesFlag |
                kClientChunkMeshClearFluidBlocksFlag;
  chunk.opaque_face_offset = mesh_frame.opaque_faces.size();
  chunk.opaque_face_count = static_cast<uint32_t>(faces.size());
  chunk.opaque_byte_count =
      static_cast<uint64_t>(faces.size()) * sizeof(uint64_t);

  mesh_frame.opaque_faces.insert(mesh_frame.opaque_faces.end(), faces.begin(),
                                 faces.end());
  mesh_frame.opaque_bytes += chunk.opaque_byte_count;
  mesh_frame.chunks.push_back(chunk);
}

void append_native_empty_cube_faces(std::vector<uint64_t> &faces,
                                    uint32_t local_x, uint32_t local_y,
                                    uint32_t local_z) {
  for (uint32_t direction = 0u; direction < 6u; ++direction) {
    faces.push_back(pack_native_empty_block_face(local_x, local_y, local_z,
                                                direction, 1u, 1u));
  }
}

void append_native_empty_world_block_face(world_mesh_upload_frame &mesh_frame,
                                          int32_t world_x, int32_t world_y,
                                          int32_t world_z,
                                          uint32_t direction) {
  std::vector<uint64_t> faces;
  faces.push_back(pack_native_empty_block_face(
      static_cast<uint32_t>(floor_mod_int32(world_x, kNativeEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_y, kNativeEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_z, kNativeEmptyWorldChunkSize)),
      direction, 1u, 1u));
  append_native_empty_chunk_faces(
      mesh_frame, floor_div_int32(world_x, kNativeEmptyWorldChunkSize),
      floor_div_int32(world_y, kNativeEmptyWorldChunkSize),
      floor_div_int32(world_z, kNativeEmptyWorldChunkSize), faces);
}

void append_native_empty_exposed_air_faces(world_mesh_upload_frame &mesh_frame,
                                           const block_lookup &overrides,
                                           const block_position_key &air) {
  struct neighbor_face {
    int32_t dx;
    int32_t dy;
    int32_t dz;
    uint32_t direction;
  };
  constexpr std::array<neighbor_face, 6> neighbors{{
      {0, 0, -1, 0u},
      {0, 0, 1, 1u},
      {-1, 0, 0, 2u},
      {1, 0, 0, 3u},
      {0, -1, 0, 4u},
      {0, 1, 0, 5u},
  }};

  for (const neighbor_face &neighbor : neighbors) {
    const block_position_key solid{
        air.x + neighbor.dx,
        air.y + neighbor.dy,
        air.z + neighbor.dz,
    };
    if (native_empty_effective_block(overrides, solid) == 0u) {
      continue;
    }

    append_native_empty_world_block_face(mesh_frame, solid.x, solid.y, solid.z,
                                         neighbor.direction);
  }
}

void apply_native_empty_overrides_from_records(
    const std::vector<world_block_record> &records, block_lookup &overrides) {
  for (const world_block_record &record : records) {
    if (record.y < kNativeEmptyWorldMinY ||
        record.y >= kNativeEmptyWorldMaxYExclusive) {
      continue;
    }

    overrides[block_position_key{record.x, record.y, record.z}] = record.block;
  }
}

bool native_empty_world_chunk_range(
    const octaryn_client_chunk_view &chunk_view, int32_t &min_chunk_x,
    int32_t &max_chunk_x, int32_t &min_chunk_z, int32_t &max_chunk_z) {
  min_chunk_x = chunk_view.origin_x;
  max_chunk_x = chunk_view.origin_x + chunk_view.width;
  min_chunk_z = chunk_view.origin_z;
  max_chunk_z = chunk_view.origin_z + chunk_view.width;
  return min_chunk_x < max_chunk_x && min_chunk_z < max_chunk_z;
}

size_t native_empty_world_chunk_count(
    const octaryn_client_chunk_view &chunk_view) {
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!native_empty_world_chunk_range(chunk_view, min_chunk_x, max_chunk_x,
                                      min_chunk_z, max_chunk_z)) {
    return 0u;
  }
  return static_cast<size_t>(max_chunk_x - min_chunk_x) *
         static_cast<size_t>(max_chunk_z - min_chunk_z);
}

size_t native_empty_world_chunk_overlap(
    const octaryn_client_chunk_view &left,
    const octaryn_client_chunk_view &right) {
  int32_t left_min_x = 0;
  int32_t left_max_x = 0;
  int32_t left_min_z = 0;
  int32_t left_max_z = 0;
  int32_t right_min_x = 0;
  int32_t right_max_x = 0;
  int32_t right_min_z = 0;
  int32_t right_max_z = 0;
  if (!native_empty_world_chunk_range(left, left_min_x, left_max_x, left_min_z,
                                      left_max_z) ||
      !native_empty_world_chunk_range(right, right_min_x, right_max_x,
                                      right_min_z, right_max_z)) {
    return 0u;
  }
  const int32_t min_x = std::max(left_min_x, right_min_x);
  const int32_t max_x = std::min(left_max_x, right_max_x);
  const int32_t min_z = std::max(left_min_z, right_min_z);
  const int32_t max_z = std::min(left_max_z, right_max_z);
  if (min_x >= max_x || min_z >= max_z) {
    return 0u;
  }
  return static_cast<size_t>(max_x - min_x) *
         static_cast<size_t>(max_z - min_z);
}

void build_native_empty_world_mesh_frame(
    const octaryn_client_chunk_view &chunk_view,
    const octaryn_client_chunk_view &previous_chunk_view,
    const block_lookup &overrides,
    world_mesh_upload_frame &mesh_frame) {
  mesh_frame = {};
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!native_empty_world_chunk_range(chunk_view, min_chunk_x, max_chunk_x,
                                      min_chunk_z, max_chunk_z)) {
    if (g_log != nullptr) {
      const size_t previous_count =
          native_empty_world_chunk_count(previous_chunk_view);
      std::fprintf(g_log,
                   "native_empty_chunk_stream active=1 loaded=0 "
                   "preserved=0 unloaded=%zu reason=outside_bounds "
                   "render_distance=%d source=client_native_unbounded\n",
                   previous_count, chunk_view.width / 2);
      std::fflush(g_log);
    }
    return;
  }

  const size_t chunk_count =
      static_cast<size_t>(max_chunk_x - min_chunk_x) *
      static_cast<size_t>(max_chunk_z - min_chunk_z);
  mesh_frame.chunks.reserve(chunk_count);
  mesh_frame.opaque_faces.reserve(chunk_count);

  for (int32_t chunk_z = min_chunk_z; chunk_z < max_chunk_z; ++chunk_z) {
    for (int32_t chunk_x = min_chunk_x; chunk_x < max_chunk_x; ++chunk_x) {
      for (int32_t chunk_y = kNativeEmptyWorldMinChunkY;
           chunk_y <= kNativeEmptyWorldChunkY; ++chunk_y) {
        std::vector<uint64_t> volume_faces;
        if (chunk_y == kNativeEmptyWorldMinChunkY) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 5u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_x == min_chunk_x) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 3u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_x == max_chunk_x - 1) {
          volume_faces.push_back(pack_native_empty_block_face(
              kNativeEmptyWorldChunkSize - 1u, 0u, 0u, 2u,
              kNativeEmptyWorldChunkSize, kNativeEmptyWorldChunkSize));
        }
        if (chunk_z == min_chunk_z) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 1u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_z == max_chunk_z - 1) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, kNativeEmptyWorldChunkSize - 1u, 0u,
              kNativeEmptyWorldChunkSize, kNativeEmptyWorldChunkSize));
        }
        append_native_empty_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                        volume_faces);
      }

      std::array<bool, kNativeEmptyWorldChunkSize * kNativeEmptyWorldChunkSize>
          hidden_top{};
      bool has_surface_override = false;
      for (const auto &entry : overrides) {
        const block_position_key &key = entry.first;
        if (key.y != -1 ||
            floor_div_int32(key.x, kNativeEmptyWorldChunkSize) != chunk_x ||
            floor_div_int32(key.z, kNativeEmptyWorldChunkSize) != chunk_z) {
          continue;
        }

        const int32_t local_x = floor_mod_int32(key.x, kNativeEmptyWorldChunkSize);
        const int32_t local_z = floor_mod_int32(key.z, kNativeEmptyWorldChunkSize);
        hidden_top[static_cast<size_t>(local_z * kNativeEmptyWorldChunkSize +
                                       local_x)] = entry.second == 0u;
        has_surface_override = true;
      }

      std::vector<uint64_t> faces;
      if (!has_surface_override) {
        faces.push_back(pack_native_empty_block_face(
            0u, kNativeEmptyWorldLocalY, 0u, 4u, kNativeEmptyWorldChunkSize,
            kNativeEmptyWorldChunkSize));
      } else {
        faces.reserve(kNativeEmptyWorldChunkSize * kNativeEmptyWorldChunkSize);
        for (uint32_t local_z = 0u; local_z < kNativeEmptyWorldChunkSize;
             ++local_z) {
          for (uint32_t local_x = 0u; local_x < kNativeEmptyWorldChunkSize;
               ++local_x) {
            if (hidden_top[static_cast<size_t>(
                    local_z * kNativeEmptyWorldChunkSize + local_x)]) {
              continue;
            }

            faces.push_back(pack_native_empty_block_face(
                local_x, kNativeEmptyWorldLocalY, local_z, 4u, 1u, 1u));
          }
        }
      }

      append_native_empty_chunk_faces(mesh_frame, chunk_x,
                                      kNativeEmptyWorldChunkY, chunk_z, faces);
    }
  }

  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (entry.second == 0u &&
        native_empty_generated_block(key) != 0u) {
      append_native_empty_exposed_air_faces(mesh_frame, overrides, key);
      continue;
    }

    if (entry.second == 0u || key.y < 0 ||
        key.y >= kNativeEmptyWorldMaxYExclusive) {
      continue;
    }

    const int32_t chunk_x = floor_div_int32(key.x, kNativeEmptyWorldChunkSize);
    const int32_t chunk_y = floor_div_int32(key.y, kNativeEmptyWorldChunkSize);
    const int32_t chunk_z = floor_div_int32(key.z, kNativeEmptyWorldChunkSize);
    if (chunk_x < min_chunk_x || chunk_x >= max_chunk_x ||
        chunk_z < min_chunk_z || chunk_z >= max_chunk_z) {
      continue;
    }

    std::vector<uint64_t> faces;
    append_native_empty_cube_faces(
        faces,
        static_cast<uint32_t>(floor_mod_int32(key.x, kNativeEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.y, kNativeEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.z, kNativeEmptyWorldChunkSize)));
    append_native_empty_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                    faces);
  }

  if (g_log != nullptr) {
    const size_t previous_count =
        native_empty_world_chunk_count(previous_chunk_view);
    const size_t preserved =
        native_empty_world_chunk_overlap(previous_chunk_view, chunk_view);
    const size_t loaded = mesh_frame.chunks.size() - preserved;
    const size_t unloaded = previous_count - preserved;
    std::fprintf(g_log,
                 "native_empty_chunk_stream active=1 source=client_native "
                 "render_distance=%d mode=unbounded_flat y=0 "
                 "loaded=%zu preserved=%zu unloaded=%zu visible_chunks=%zu "
                 "override_edits=%zu opaque_faces=%zu\n",
                 chunk_view.width / 2, loaded, preserved, unloaded,
                 mesh_frame.chunks.size(), overrides.size(),
                 mesh_frame.opaque_faces.size());
    std::fflush(g_log);
  }
}

void build_native_empty_world_mesh_frame_from_stream(
    const server_chunk_stream_file &stream, const block_lookup &overrides,
    const octaryn_client_chunk_view &previous_chunk_view,
    world_mesh_upload_frame &mesh_frame) {
  const octaryn_client_chunk_view stream_view =
      chunk_view_from_server_stream(stream);
  build_native_empty_world_mesh_frame(stream_view, previous_chunk_view,
                                      overrides, mesh_frame);

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "native_empty_chunk_stream active=1 source=server_background "
                 "epoch=%" PRIu64 " render_distance=%" PRIu32
                 " columns=%zu override_edits=%zu visible_chunks=%zu "
                 "opaque_faces=%zu\n",
                 stream.epoch, stream.radius, stream.columns.size(),
                 overrides.size(), mesh_frame.chunks.size(),
                 mesh_frame.opaque_faces.size());
    std::fflush(g_log);
  }
}

int32_t clamp_int32(int32_t value, int32_t minimum, int32_t maximum) {
  return std::min(std::max(value, minimum), maximum);
}

ui_uniforms build_ui_uniforms(
    const ClientBlockAtlas &atlas,
    const octaryn_client_runtime_controls &controls,
    const octaryn_client_frame_profile_snapshot &profile,
    uint16_t selected_place_block) {
  ui_uniforms uniforms{};
  const int32_t selected_layer =
      client_block_atlas_top_layer_for_block(atlas, selected_place_block);
  uniforms.index = selected_layer > 0 ? static_cast<uint32_t>(selected_layer) : 0u;
  uniforms.debug_enabled = controls.debug_overlay_enabled != 0u ? 1u : 0u;
  uniforms.fps_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.current.fps);
  uniforms.frame_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.current.ms);
  uniforms.profile_frame_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.total_ms);
  uniforms.fps_average_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.average.fps);
  uniforms.fps_low_1_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.low_1pct.fps);
  uniforms.fps_low_0_1_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.low_0_1pct.fps);
  uniforms.fps_low_x5_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.confirmed_low_5.fps);
  uniforms.fps_low_x10_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.confirmed_low_10.fps);
  uniforms.fps_worst_tenths =
      octaryn_client_frame_profile_tenths_from_fps(profile.metrics.worst.fps);
  uniforms.warmup_complete = profile.metrics.warmup_complete;
  uniforms.sample_count =
      profile.metrics.sample_count > UINT32_MAX
          ? UINT32_MAX
          : static_cast<uint32_t>(profile.metrics.sample_count);
  uniforms.ms_low_1_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.low_1pct.ms);
  uniforms.ms_low_0_1_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.low_0_1pct.ms);
  uniforms.ms_low_x5_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.confirmed_low_5.ms);
  uniforms.ms_low_x10_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.confirmed_low_10.ms);
  uniforms.ms_worst_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.metrics.worst.ms);
  uniforms.warmup_elapsed_hundredths =
      octaryn_client_frame_profile_hundredths_from_seconds(
          profile.metrics.warmup_elapsed_seconds);
  uniforms.warmup_total_hundredths =
      octaryn_client_frame_profile_hundredths_from_seconds(
          profile.metrics.warmup_seconds);
  uniforms.sim_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.sim_ms);
  uniforms.misc_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.misc_ms);
  uniforms.world_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.world_ms);
  uniforms.render_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.render_ms);
  uniforms.render_setup_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.render_setup_ms);
  uniforms.render_other_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.render_other_ms);
  uniforms.gbuffer_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.gbuffer_ms);
  uniforms.gbuffer_sky_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.gbuffer_sky_ms);
  uniforms.gbuffer_opaque_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.gbuffer_opaque_ms);
  uniforms.gbuffer_sprite_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.gbuffer_sprite_ms);
  uniforms.post_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.post_ms);
  uniforms.composite_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.composite_ms);
  uniforms.depth_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.depth_ms);
  uniforms.forward_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.forward_ms);
  uniforms.ui_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.ui_ms);
  uniforms.imgui_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.imgui_ms);
  uniforms.swapchain_blit_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.swapchain_blit_ms);
  uniforms.render_submit_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.render_submit_ms);
  uniforms.untracked_time_hundredths =
      octaryn_client_frame_profile_hundredths_from_ms(profile.sample.untracked_ms);
  uniforms.menu_enabled = controls.display_menu.active != 0u ? 1u : 0u;
  uniforms.menu_row = static_cast<uint32_t>(
      clamp_int32(controls.display_menu.row, 0,
                  OCTARYN_CLIENT_DISPLAY_MENU_ROW_COUNT - 1));
  uniforms.menu_display =
      controls.display_menu.display_index >= 0
          ? static_cast<uint32_t>(controls.display_menu.display_index + 1)
          : 0u;
  if (controls.display_menu.mode_index >= 0 &&
      controls.display_menu.mode_index < controls.display_catalog.mode_count) {
    const octaryn_client_display_catalog_mode &mode =
        controls.display_catalog.modes[controls.display_menu.mode_index];
    uniforms.menu_mode_width = static_cast<uint32_t>(mode.pixel_width);
    uniforms.menu_mode_height = static_cast<uint32_t>(mode.pixel_height);
  }
  uniforms.menu_fullscreen = controls.display_menu.fullscreen;
  uniforms.menu_present_mode =
      controls.display_menu.present_mode_index >= 0
          ? static_cast<uint32_t>(controls.display_menu.present_mode_index)
          : 0u;
  uniforms.menu_fog = controls.display_menu.fog_enabled;
  const int *distance_options = octaryn_client_render_distance_options();
  const int distance_count = octaryn_client_render_distance_option_count();
  if (controls.display_menu.render_distance_index >= 0 &&
      controls.display_menu.render_distance_index < distance_count) {
    uniforms.menu_render_distance = static_cast<uint32_t>(
        distance_options[controls.display_menu.render_distance_index]);
  } else {
    uniforms.menu_render_distance =
        static_cast<uint32_t>(controls.render_distance);
  }
  uniforms.menu_clouds = controls.display_menu.clouds_enabled;
  uniforms.menu_sky_gradient = controls.display_menu.sky_gradient_enabled;
  uniforms.menu_stars = controls.display_menu.stars_enabled;
  uniforms.menu_sun = controls.display_menu.sun_enabled;
  uniforms.menu_moon = controls.display_menu.moon_enabled;
  uniforms.menu_pom = controls.display_menu.pom_enabled;
  uniforms.menu_pbr = controls.display_menu.pbr_enabled;
  return uniforms;
}

void dispatch_ui_rect(SDL_GPUCommandBuffer *command_buffer,
                      SDL_GPUComputePass *compute_pass,
                      int32_t viewport_width, int32_t viewport_height,
                      int32_t x, int32_t y, int32_t width, int32_t height) {
  const int32_t x0 = clamp_int32(x, 0, viewport_width);
  const int32_t y0 = clamp_int32(y, 0, viewport_height);
  const int32_t x1 = clamp_int32(x + width, 0, viewport_width);
  const int32_t y1 = clamp_int32(y + height, 0, viewport_height);
  if (x1 <= x0 || y1 <= y0) {
    return;
  }

  ui_viewport_uniforms viewport_uniform{};
  viewport_uniform.viewport[0] = viewport_width;
  viewport_uniform.viewport[1] = viewport_height;
  viewport_uniform.offset[0] = x0;
  viewport_uniform.offset[1] = y0;
  const uint32_t groups_x = static_cast<uint32_t>((x1 - x0 + 7) / 8);
  const uint32_t groups_y = static_cast<uint32_t>((y1 - y0 + 7) / 8);
  SDL_PushGPUComputeUniformData(command_buffer, 0u, &viewport_uniform,
                                sizeof(viewport_uniform));
  SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1u);
}

void dispatch_ui_regions(SDL_GPUCommandBuffer *command_buffer,
                         SDL_GPUComputePass *compute_pass,
                         const ui_uniforms &uniforms,
                         int32_t viewport_width, int32_t viewport_height) {
  if (viewport_width <= 0 || viewport_height <= 0) {
    return;
  }

  if (uniforms.menu_enabled != 0u) {
    dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                     viewport_height, 0, 0, viewport_width, viewport_height);
    return;
  }

  const float base_scale = std::max(static_cast<float>(viewport_width) / 1280.0f,
                                    static_cast<float>(viewport_height) / 720.0f);
  const float scale = base_scale * 2.0f;
  const int32_t block_start = static_cast<int32_t>(std::floor(10.0f * scale)) - 2;
  const int32_t block_end = static_cast<int32_t>(std::ceil(60.0f * scale)) + 2;
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, block_start,
                   viewport_height - block_end, block_end - block_start,
                   block_end - block_start);

  const int32_t cross_width =
      static_cast<int32_t>(std::ceil(8.0f * base_scale)) + 2;
  const int32_t cross_thickness =
      static_cast<int32_t>(std::ceil(2.0f * base_scale)) + 2;
  const int32_t center_x = viewport_width / 2;
  const int32_t center_y = viewport_height / 2;
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, center_x - cross_width,
                   center_y - cross_thickness, cross_width * 2,
                   cross_thickness * 2);
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, center_x - cross_thickness,
                   center_y - cross_width, cross_thickness * 2,
                   cross_width * 2);

  if (uniforms.debug_enabled == 0u) {
    return;
  }

  const uint32_t font_scale =
      std::max(1u, static_cast<uint32_t>(scale + 0.5f));
  const int32_t padding = 4 * static_cast<int32_t>(font_scale);
  const int32_t margin = 6 * static_cast<int32_t>(font_scale);
  const int32_t content_width = 30 * 4 * static_cast<int32_t>(font_scale) -
                                static_cast<int32_t>(font_scale);
  const int32_t content_height = 16 * 6 * static_cast<int32_t>(font_scale) -
                                 static_cast<int32_t>(font_scale);
  dispatch_ui_rect(command_buffer, compute_pass, viewport_width,
                   viewport_height, margin, margin, content_width + padding * 2,
                   content_height + padding * 2);
}

bool render_ui_overlay(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUTexture *target_texture,
    const ClientBlockAtlas &atlas,
    const client_shader_pipelines &pipelines,
    const octaryn_client_runtime_controls &controls,
    const octaryn_client_frame_profile_snapshot &profile,
    uint16_t selected_place_block,
    uint32_t target_width,
    uint32_t target_height) {
  if (pipelines.ui == nullptr || pipelines.atlas_sampler == nullptr ||
      target_texture == nullptr || atlas.color_texture == nullptr) {
    return true;
  }

  SDL_GPUStorageTextureReadWriteBinding write_textures[1]{};
  write_textures[0].texture = target_texture;
  SDL_GPUComputePass *compute_pass = SDL_BeginGPUComputePass(
      command_buffer, write_textures, 1u, nullptr, 0u);
  if (compute_pass == nullptr) {
    log_line("live_ui_compute_pass=failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding read_textures[1]{};
  read_textures[0].texture = atlas.color_texture;
  read_textures[0].sampler = pipelines.atlas_sampler;
  const ui_uniforms uniforms =
      build_ui_uniforms(atlas, controls, profile, selected_place_block);
  SDL_BindGPUComputePipeline(compute_pass, pipelines.ui);
  SDL_BindGPUComputeSamplers(compute_pass, 0u, read_textures, 1u);
  SDL_PushGPUComputeUniformData(command_buffer, 1u, &uniforms,
                                sizeof(uniforms));
  dispatch_ui_regions(command_buffer, compute_pass, uniforms,
                      static_cast<int32_t>(target_width),
                      static_cast<int32_t>(target_height));
  SDL_EndGPUComputePass(compute_pass);
  if (g_log != nullptr &&
      (uniforms.debug_enabled != 0u || uniforms.menu_enabled != 0u)) {
    std::fprintf(g_log,
                 "live_ui_overlay active=1 debug=%" PRIu32 " menu=%" PRIu32
                 " row=%" PRIu32 " render_distance=%" PRIu32 "\n",
                 uniforms.debug_enabled, uniforms.menu_enabled,
                 uniforms.menu_row, uniforms.menu_render_distance);
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
  upload_frame.opaque_faces.assign(scratch.opaque_faces.begin(),
                                   scratch.opaque_faces.begin() +
                                       opaque_faces_written);
  upload_frame.transparent_faces.assign(scratch.transparent_faces.begin(),
                                        scratch.transparent_faces.begin() +
                                            transparent_faces_written);
  upload_frame.sprite_vertices.assign(scratch.sprite_vertices.begin(),
                                      scratch.sprite_vertices.begin() +
                                          sprite_vertices_written);
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

bool same_chunk_mesh(const octaryn_client_chunk_mesh_upload_record &left,
                     const octaryn_client_chunk_mesh_upload_record &right) {
  return left.chunk_x == right.chunk_x && left.chunk_y == right.chunk_y &&
         left.chunk_z == right.chunk_z;
}

bool chunk_mesh_has_geometry(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
         chunk.sprite_vertex_count != 0u;
}

bool chunk_mesh_update_contains(
    const world_mesh_upload_frame &update,
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  for (const octaryn_client_chunk_mesh_upload_record &update_chunk :
       update.chunks) {
    if (same_chunk_mesh(update_chunk, chunk)) {
      return true;
    }
  }
  return false;
}

void append_chunk_mesh(world_mesh_upload_frame &destination,
                       const world_mesh_upload_frame &source,
                       const octaryn_client_chunk_mesh_upload_record &chunk) {
  octaryn_client_chunk_mesh_upload_record copied = chunk;
  copied.opaque_face_offset = destination.opaque_faces.size();
  copied.transparent_face_offset = destination.transparent_faces.size();
  copied.sprite_vertex_offset = destination.sprite_vertices.size();

  const auto opaque_begin =
      source.opaque_faces.begin() +
      static_cast<std::ptrdiff_t>(chunk.opaque_face_offset);
  destination.opaque_faces.insert(
      destination.opaque_faces.end(), opaque_begin,
      opaque_begin + static_cast<std::ptrdiff_t>(chunk.opaque_face_count));

  const auto transparent_begin =
      source.transparent_faces.begin() +
      static_cast<std::ptrdiff_t>(chunk.transparent_face_offset);
  destination.transparent_faces.insert(
      destination.transparent_faces.end(), transparent_begin,
      transparent_begin +
          static_cast<std::ptrdiff_t>(chunk.transparent_face_count));

  const auto sprite_begin =
      source.sprite_vertices.begin() +
      static_cast<std::ptrdiff_t>(chunk.sprite_vertex_offset);
  destination.sprite_vertices.insert(
      destination.sprite_vertices.end(), sprite_begin,
      sprite_begin + static_cast<std::ptrdiff_t>(chunk.sprite_vertex_count));

  copied.opaque_byte_count =
      static_cast<uint64_t>(copied.opaque_face_count) * sizeof(uint64_t);
  copied.transparent_byte_count =
      static_cast<uint64_t>(copied.transparent_face_count) * sizeof(uint64_t);
  copied.sprite_byte_count =
      static_cast<uint64_t>(copied.sprite_vertex_count) * sizeof(uint32_t);
  destination.opaque_bytes += copied.opaque_byte_count;
  destination.transparent_bytes += copied.transparent_byte_count;
  destination.sprite_bytes += copied.sprite_byte_count;
  destination.fluid_blocks += copied.fluid_block_count;
  destination.chunks.push_back(copied);
}

void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index) {
  if (update_frame.chunks.empty()) {
    return;
  }

  world_mesh_upload_frame merged{};
  merged.chunks.reserve(visible_frame.chunks.size() + update_frame.chunks.size());
  merged.opaque_faces.reserve(visible_frame.opaque_faces.size() +
                              update_frame.opaque_faces.size());
  merged.transparent_faces.reserve(visible_frame.transparent_faces.size() +
                                   update_frame.transparent_faces.size());
  merged.sprite_vertices.reserve(visible_frame.sprite_vertices.size() +
                                 update_frame.sprite_vertices.size());

  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       visible_frame.chunks) {
    if (!chunk_mesh_update_contains(update_frame, chunk)) {
      append_chunk_mesh(merged, visible_frame, chunk);
    }
  }

  uint32_t removed_chunks = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    if (chunk_mesh_has_geometry(chunk)) {
      append_chunk_mesh(merged, update_frame, chunk);
    } else {
      ++removed_chunks;
    }
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_chunk_mesh_retained frame=%" PRIu64
                 " active=1 updates=%zu retained=%zu removed=%" PRIu32
                 " visible_chunks=%zu opaque_faces=%zu transparent_faces=%zu"
                 " sprite_vertices=%zu\n",
                 frame_index, update_frame.chunks.size(),
                 visible_frame.chunks.size(), removed_chunks,
                 merged.chunks.size(), merged.opaque_faces.size(),
                 merged.transparent_faces.size(), merged.sprite_vertices.size());
    std::fflush(g_log);
  }

  visible_frame = std::move(merged);
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

  if (!upload_gpu_buffer(device, upload_frame.opaque_faces.data(),
                         upload_frame.opaque_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_opaque", buffers.opaque_faces) ||
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
  const float length = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
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
                                const octaryn_client_camera &camera,
                                const octaryn_client_runtime_controls &controls) {
  const float day_fraction = clamp01(world_time.day_fraction);
  const float angle = day_fraction * kPi * 2.0f - kPi * 0.5f;
  float sun_direction[3] = {
      std::cos(angle),
      std::sin(angle),
      0.0f,
  };
  normalize3(sun_direction);

  const float day_visibility = smootherstep(-0.10f, 0.25f, sun_direction[1]);
  const float twilight = smootherstep(-0.28f, 0.02f, sun_direction[1]) *
                         (1.0f - smootherstep(0.06f, 0.36f, sun_direction[1]));
  sky_uniforms uniforms{};
  uniforms.light_direction_and_sky_visibility[0] = -sun_direction[0];
  uniforms.light_direction_and_sky_visibility[1] = -sun_direction[1];
  uniforms.light_direction_and_sky_visibility[2] = -sun_direction[2];
  uniforms.light_direction_and_sky_visibility[3] =
      std::max(0.08f, day_visibility);
  uniforms.twilight_celestial_cloud_time[0] = twilight;
  uniforms.twilight_celestial_cloud_time[1] = day_visibility;
  uniforms.twilight_celestial_cloud_time[2] =
      controls.sky_gradient_enabled != 0u ? 1.0f : 0.0f;
  uniforms.twilight_celestial_cloud_time[3] =
      static_cast<float>(std::fmod(world_time.total_seconds, 86400.0));
  uniforms.camera_position_and_cloud_height[0] = camera.position[0];
  uniforms.camera_position_and_cloud_height[1] = camera.position[1];
  uniforms.camera_position_and_cloud_height[2] = camera.position[2];
  uniforms.camera_position_and_cloud_height[3] = 192.0f;
  uniforms.celestial_toggles[0] =
      controls.stars_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[1] =
      controls.sun_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[2] =
      controls.moon_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[3] = 0.0f;
  return uniforms;
}

matrix_uniform matrix_from_camera_values(const float values[4][4]) {
  matrix_uniform output{};
  std::memcpy(output.values, values, sizeof(output.values));
  return output;
}

camera_uniforms
camera_uniform_from_camera(const octaryn_client_camera &camera) {
  camera_uniforms uniforms{};
  uniforms.position[0] = camera.position[0];
  uniforms.position[1] = camera.position[1];
  uniforms.position[2] = camera.position[2];
  uniforms.position[3] = 1.0f;
  return uniforms;
}

bool draw_shader_world(SDL_GPUCommandBuffer *command_buffer,
                       SDL_GPUTexture *target_texture,
                       SDL_GPUTexture *depth_texture,
                       SDL_GPUTexture *position_texture,
                       SDL_GPUTexture *voxel_texture,
                       SDL_GPUTexture *material_texture,
                       const ClientBlockAtlas &atlas,
                       const client_shader_pipelines &pipelines,
                       const world_mesh_gpu_buffers &mesh_buffers,
                       const world_mesh_upload_frame &mesh_frame,
                       const octaryn_client_camera &camera,
                       const client_block_raycast_hit &selection_hit,
                       const server_world_time_state &world_time,
                       const octaryn_client_runtime_controls &controls,
                       uint64_t frame_index,
                       octaryn_client_frame_profile_sample *profile_sample) {
  if (pipelines.sky == nullptr) {
    return true;
  }

  octaryn_client_function_profile_scope sky_profile_scope(
      "sky_pass", frame_index, "sdl_gpu_commands");
  const uint64_t sky_start = SDL_GetTicksNS();
  SDL_GPUColorTargetInfo target{};
  target.texture = target_texture;
  target.load_op = SDL_GPU_LOADOP_LOAD;
  target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *sky_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1u, nullptr);
  if (sky_pass == nullptr) {
    log_line("live_sky_render_pass=failed");
    return false;
  }

  const matrix_uniform projection =
      matrix_from_camera_values(camera.projection);
  const matrix_uniform view = matrix_from_camera_values(camera.view);
  const sky_uniforms sky = build_sky_uniforms(world_time, camera, controls);
  const camera_uniforms camera_uniform = camera_uniform_from_camera(camera);

  SDL_PushGPUVertexUniformData(command_buffer, 0u, &projection,
                               sizeof(projection));
  SDL_PushGPUVertexUniformData(command_buffer, 1u, &view, sizeof(view));
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &sky, sizeof(sky));
  SDL_BindGPUGraphicsPipeline(sky_pass, pipelines.sky);
  SDL_DrawGPUPrimitives(sky_pass, 36u, 1u, 0u, 0u);
  SDL_EndGPURenderPass(sky_pass);
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_sky_ms =
        octaryn_client_frame_profile_elapsed_ms_since(sky_start);
  }
  if (!g_sky_path_logged && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_sky_pass active=1 source=server_world_time "
                 "day_fraction=%.6f total_seconds=%.3f\n",
                 world_time.day_fraction, world_time.total_seconds);
    std::fflush(g_log);
    g_sky_path_logged = true;
  }

  const bool world_ready =
      pipelines.world != nullptr && pipelines.atlas_sampler != nullptr &&
      atlas.color_texture != nullptr && atlas.normal_texture != nullptr &&
      atlas.specular_texture != nullptr && mesh_buffers.opaque_faces != nullptr &&
      !mesh_frame.opaque_faces.empty();
  if (!world_ready) {
    return true;
  }

  SDL_GPUColorTargetInfo world_targets[4]{};
  world_targets[0].texture = target_texture;
  world_targets[0].load_op = SDL_GPU_LOADOP_LOAD;
  world_targets[0].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[1].texture = position_texture;
  world_targets[1].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[1].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[2].texture = voxel_texture;
  world_targets[2].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[2].store_op = SDL_GPU_STOREOP_STORE;
  world_targets[3].texture = material_texture;
  world_targets[3].load_op = SDL_GPU_LOADOP_CLEAR;
  world_targets[3].store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = depth_texture;
  depth.load_op = SDL_GPU_LOADOP_CLEAR;
  depth.store_op = SDL_GPU_STOREOP_STORE;
  depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth.clear_depth = 1.0f;
  SDL_GPURenderPass *world_pass =
      SDL_BeginGPURenderPass(command_buffer, world_targets, 4u, &depth);
  if (world_pass == nullptr) {
    log_line("live_world_render_pass=failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding atlas_bindings[3]{};
  atlas_bindings[0].texture = atlas.color_texture;
  atlas_bindings[0].sampler = pipelines.atlas_sampler;
  atlas_bindings[1].texture = atlas.normal_texture;
  atlas_bindings[1].sampler = pipelines.atlas_sampler;
  atlas_bindings[2].texture = atlas.specular_texture;
  atlas_bindings[2].sampler = pipelines.atlas_sampler;
  SDL_BindGPUFragmentSamplers(world_pass, 0u, atlas_bindings, 3u);
  SDL_GPUBuffer *storage_buffers[2] = {
      mesh_buffers.opaque_faces,
      mesh_buffers.opaque_faces,
  };
  SDL_BindGPUVertexStorageBuffers(world_pass, 0u, storage_buffers, 2u);
  SDL_PushGPUVertexUniformData(command_buffer, 3u, &camera_uniform,
                               sizeof(camera_uniform));
  opaque_fragment_uniforms fragment_uniforms{};
  fragment_uniforms.skylight_floor = 0.24f;
  fragment_uniforms.cloud_time_seconds =
      static_cast<float>(std::fmod(world_time.total_seconds, 86400.0));
  fragment_uniforms.sky_visibility =
      sky.light_direction_and_sky_visibility[3];
  fragment_uniforms.twilight_strength =
      sky.twilight_celestial_cloud_time[0];
  fragment_uniforms.celestial_visibility =
      sky.twilight_celestial_cloud_time[1];
  fragment_uniforms.material_flags =
      (controls.pom_enabled != 0u ? 0x1u : 0u) |
      (controls.pbr_enabled != 0u ? 0x2u : 0u);
  fragment_uniforms.camera_position[0] = camera.position[0];
  fragment_uniforms.camera_position[1] = camera.position[1];
  fragment_uniforms.camera_position[2] = camera.position[2];
  fragment_uniforms.camera_position[3] = 1.0f;
  hidden_block_uniforms hidden_uniforms{};
  block_highlight_uniforms highlight_uniforms{};
  if (selection_hit.has_hit) {
    highlight_uniforms.highlight_block_enabled = 1u;
    highlight_uniforms.highlight_block[0] = selection_hit.hit.x;
    highlight_uniforms.highlight_block[1] = selection_hit.hit.y;
    highlight_uniforms.highlight_block[2] = selection_hit.hit.z;
    highlight_uniforms.highlight_block[3] = selection_hit.block;
  }
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &fragment_uniforms,
                                 sizeof(fragment_uniforms));
  SDL_PushGPUFragmentUniformData(command_buffer, 1u, &hidden_uniforms,
                                 sizeof(hidden_uniforms));
  SDL_PushGPUFragmentUniformData(command_buffer, 2u, &highlight_uniforms,
                                 sizeof(highlight_uniforms));
  SDL_BindGPUGraphicsPipeline(world_pass, pipelines.world);

  const uint64_t opaque_start = SDL_GetTicksNS();
  uint32_t drawn_chunks = 0u;
  uint64_t drawn_faces = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       mesh_frame.chunks) {
    if (chunk.opaque_face_count == 0u) {
      continue;
    }

    chunk_uniforms chunk_uniform{};
    chunk_uniform.chunk_position[0] = chunk.chunk_x * 32;
    chunk_uniform.chunk_position[1] = chunk.chunk_y * 32;
    chunk_uniform.chunk_position[2] = chunk.chunk_z * 32;
    chunk_uniform.chunk_position[3] = 0;
    chunk_uniform.face_offset = static_cast<uint32_t>(chunk.opaque_face_offset);
    chunk_uniform.draw_flags = kDrawFlagUseFaceBuffer;
    SDL_PushGPUVertexUniformData(command_buffer, 2u, &chunk_uniform,
                                 sizeof(chunk_uniform));
    SDL_DrawGPUPrimitives(world_pass, chunk.opaque_face_count * 6u, 1u, 0u,
                          0u);
    ++drawn_chunks;
    drawn_faces += chunk.opaque_face_count;
  }
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_opaque_ms =
        octaryn_client_frame_profile_elapsed_ms_since(opaque_start);
  }

  const uint64_t sprite_start = SDL_GetTicksNS();
  SDL_GPUBuffer *sprite_storage_buffers[2] = {
      mesh_buffers.sprite_vertices,
      mesh_buffers.opaque_faces,
  };
  if (mesh_buffers.sprite_vertices != nullptr) {
    SDL_BindGPUVertexStorageBuffers(world_pass, 0u, sprite_storage_buffers, 2u);
    SDL_BindGPUGraphicsPipeline(world_pass, pipelines.opaque_sprite);
  }

  uint32_t drawn_sprite_chunks = 0u;
  uint64_t drawn_sprite_indices = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       mesh_frame.chunks) {
    if (chunk.sprite_index_count == 0u) {
      continue;
    }

    chunk_uniforms chunk_uniform{};
    chunk_uniform.chunk_position[0] = chunk.chunk_x * 32;
    chunk_uniform.chunk_position[1] = chunk.chunk_y * 32;
    chunk_uniform.chunk_position[2] = chunk.chunk_z * 32;
    chunk_uniform.chunk_position[3] = 0;
    chunk_uniform.face_offset =
        static_cast<uint32_t>(chunk.sprite_vertex_offset);
    chunk_uniform.draw_flags = 0u;
    SDL_PushGPUVertexUniformData(command_buffer, 2u, &chunk_uniform,
                                 sizeof(chunk_uniform));
    SDL_DrawGPUPrimitives(world_pass, chunk.sprite_index_count, 1u, 0u, 0u);
    ++drawn_sprite_chunks;
    drawn_sprite_indices += chunk.sprite_index_count;
  }
  if (profile_sample != nullptr) {
    profile_sample->gbuffer_sprite_ms =
        octaryn_client_frame_profile_elapsed_ms_since(sprite_start);
  }

  SDL_EndGPURenderPass(world_pass);
  if (g_log != nullptr && (drawn_faces != 0u || drawn_sprite_indices != 0u)) {
    std::fprintf(g_log,
                 "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline "
                 "active=1 chunks=%" PRIu32 " opaque_faces=%" PRIu64
                 " sprite_chunks=%" PRIu32 " sprite_indices=%" PRIu64 "\n",
                 drawn_chunks, drawn_faces, drawn_sprite_chunks,
                 drawn_sprite_indices);
    if (selection_hit.has_hit) {
      std::fprintf(g_log,
                   "live_block_highlight active=1 source=opaque_texture_shader "
                   "block=(%d,%d,%d,%u)\n",
                   selection_hit.hit.x, selection_hit.hit.y, selection_hit.hit.z,
                   static_cast<unsigned>(selection_hit.block));
    }
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

void log_frame_profile(uint64_t frame_index,
                       const octaryn_client_frame_profile_snapshot &profile,
                       uint8_t debug_overlay_enabled) {
  if (g_log == nullptr) {
    return;
  }

  if (frame_index <= 5u || frame_index % 60u == 0u ||
      debug_overlay_enabled != 0u) {
    const octaryn_client_frame_profile_sample &sample = profile.sample;
    std::fprintf(
        g_log,
        "live_frame_profile frame=%" PRIu64
        " fps=%.1f avg_fps=%.1f low_1_fps=%.1f low_0_1_fps=%.1f"
        " low_x5_fps=%.1f low_x10_fps=%.1f worst_fps=%.1f"
        " frame_ms=%.3f avg_ms=%.3f low_1_ms=%.3f low_0_1_ms=%.3f"
        " low_x5_ms=%.3f low_x10_ms=%.3f worst_ms=%.3f"
        " sim_ms=%.3f misc_ms=%.3f world_ms=%.3f render_ms=%.3f"
        " setup_ms=%.3f other_ms=%.3f gbuffer_ms=%.3f sky_ms=%.3f"
        " opaque_ms=%.3f sprite_ms=%.3f post_ms=%.3f composite_ms=%.3f"
        " depth_ms=%.3f forward_ms=%.3f ui_ms=%.3f imgui_ms=%.3f"
        " blit_ms=%.3f submit_ms=%.3f acquire_ms=%.3f command_ms=%.3f"
        " swap_wait_ms=%.3f untracked_ms=%.3f warmup=%u samples=%" PRIu64 "\n",
        frame_index, profile.metrics.current.fps, profile.metrics.average.fps,
        profile.metrics.low_1pct.fps, profile.metrics.low_0_1pct.fps,
        profile.metrics.confirmed_low_5.fps,
        profile.metrics.confirmed_low_10.fps, profile.metrics.worst.fps,
        sample.total_ms, profile.metrics.average.ms,
        profile.metrics.low_1pct.ms, profile.metrics.low_0_1pct.ms,
        profile.metrics.confirmed_low_5.ms,
        profile.metrics.confirmed_low_10.ms, profile.metrics.worst.ms,
        sample.sim_ms, sample.misc_ms, sample.world_ms, sample.render_ms,
        sample.render_setup_ms, sample.render_other_ms, sample.gbuffer_ms,
        sample.gbuffer_sky_ms, sample.gbuffer_opaque_ms,
        sample.gbuffer_sprite_ms, sample.post_ms, sample.composite_ms,
        sample.depth_ms, sample.forward_ms, sample.ui_ms, sample.imgui_ms,
        sample.swapchain_blit_ms, sample.render_submit_ms,
        sample.frame_acquire_ms, sample.command_acquire_ms,
        sample.swapchain_wait_ms, sample.untracked_ms,
        static_cast<unsigned>(profile.metrics.warmup_complete),
        profile.metrics.sample_count);
    std::fflush(g_log);
  }
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

  camera.position[0] =
      (static_cast<float>(min_x) + static_cast<float>(max_x)) * 0.5f;
  camera.position[1] = static_cast<float>(min_y) + 2.0f;
  camera.position[2] =
      (static_cast<float>(min_z) + static_cast<float>(max_z)) * 0.5f;
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
    std::fprintf(
        g_log,
        "live_chunk_view frame=%" PRIu64
        " origin=(%d,%d) width=%d radius=%d source=render_distance_radius "
        "authority=server\n",
        frame_index, view.origin_x, view.origin_z, view.width, view.width / 2);
    std::fflush(g_log);
  }

  if (!write_chunk_view_intent(view, logged_view, frame_index)) {
    return;
  }

  logged_view = view;
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
                 uint32_t target_height, const ClientBlockAtlas &atlas,
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
        client_block_atlas_top_layer_for_block(atlas, block.block);
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

bool draw_material_atlas_probe(SDL_GPUCommandBuffer *command_buffer,
                               SDL_GPUTexture *target_texture,
                               uint32_t target_width, uint32_t target_height,
                               const ClientBlockAtlas &atlas) {
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
                        atlas.tile_size,
                        kMaterialAtlasProbeNormalX, kMaterialAtlasProbeY,
                        kMaterialAtlasProbeSize, target_width, target_height) ||
      !blit_gpu_texture(command_buffer, atlas.specular_texture, target_texture,
                        static_cast<uint32_t>(kMaterialAtlasProbeLayer), 0, 0,
                        atlas.tile_size,
                        kMaterialAtlasProbeSpecularX, kMaterialAtlasProbeY,
                        kMaterialAtlasProbeSize, target_width, target_height)) {
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
  const uint8_t *pixel =
      bytes + readback.y * readback.row_pitch + readback.x * readback.texel_size;
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

float build_composite_ambient_strength(float visual_sky_visibility) {
  const float gameplay_skylight = clamp01(visual_sky_visibility);
  const float ambient_scale =
      0.18f + std::pow(gameplay_skylight, 0.95f) * 0.82f;
  return 0.82f * (0.28f + ambient_scale * 0.72f);
}

composite_uniforms
build_composite_uniforms(const server_world_time_state &world_time,
                         const octaryn_client_camera &camera,
                         const octaryn_client_runtime_controls &controls) {
  const sky_uniforms sky = build_sky_uniforms(world_time, camera, controls);
  const float visual_sky_visibility =
      sky.light_direction_and_sky_visibility[3];
  composite_uniforms uniforms{};
  uniforms.sky_visibility_and_ambient_strength[0] = visual_sky_visibility;
  uniforms.sky_visibility_and_ambient_strength[1] =
      build_composite_ambient_strength(visual_sky_visibility);
  uniforms.sky_visibility_and_ambient_strength[2] = 0.0f;
  uniforms.sky_visibility_and_ambient_strength[3] = 0.0f;
  return uniforms;
}

bool run_composite_pass(SDL_GPUCommandBuffer *command_buffer,
                        SDL_GPUTexture *color_texture,
                        SDL_GPUTexture *position_texture,
                        SDL_GPUTexture *voxel_texture,
                        SDL_GPUTexture *material_texture,
                        SDL_GPUTexture *composite_texture,
                        const client_shader_pipelines &pipelines,
                        const server_world_time_state &world_time,
                        const octaryn_client_camera &camera,
                        const octaryn_client_runtime_controls &controls,
                        uint32_t target_width, uint32_t target_height,
                        uint64_t frame_index,
                        octaryn_client_frame_profile_sample *profile_sample) {
  if (pipelines.composite == nullptr || pipelines.atlas_sampler == nullptr) {
    log_line("live_composite_pass active=0 reason=missing_pipeline");
    return false;
  }

  octaryn_client_function_profile_scope composite_profile_scope(
      "composite_pass", frame_index, "sdl_gpu_commands");
  SDL_GPUStorageTextureReadWriteBinding write_texture{};
  write_texture.texture = composite_texture;
  write_texture.cycle = true;
  SDL_GPUComputePass *compute_pass =
      SDL_BeginGPUComputePass(command_buffer, &write_texture, 1u, nullptr, 0u);
  if (compute_pass == nullptr) {
    log_line("live_composite_pass active=0 reason=begin_failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding read_samplers[4]{};
  read_samplers[0].texture = color_texture;
  read_samplers[0].sampler = pipelines.atlas_sampler;
  read_samplers[1].texture = position_texture;
  read_samplers[1].sampler = pipelines.atlas_sampler;
  read_samplers[2].texture = voxel_texture;
  read_samplers[2].sampler = pipelines.atlas_sampler;
  read_samplers[3].texture = material_texture;
  read_samplers[3].sampler = pipelines.atlas_sampler;
  const composite_uniforms uniforms =
      build_composite_uniforms(world_time, camera, controls);

  const uint64_t composite_start = SDL_GetTicksNS();
  SDL_PushGPUDebugGroup(command_buffer, "composite");
  SDL_BindGPUComputePipeline(compute_pass, pipelines.composite);
  SDL_BindGPUComputeSamplers(compute_pass, 0u, read_samplers, 4u);
  SDL_PushGPUComputeUniformData(command_buffer, 0u, &uniforms,
                                sizeof(uniforms));
  SDL_DispatchGPUCompute(compute_pass, (target_width + 7u) / 8u,
                         (target_height + 7u) / 8u, 1u);
  SDL_EndGPUComputePass(compute_pass);
  SDL_PopGPUDebugGroup(command_buffer);

  if (profile_sample != nullptr) {
    profile_sample->composite_ms =
        octaryn_client_frame_profile_elapsed_ms_since(composite_start);
    profile_sample->post_ms =
        profile_sample->composite_ms + profile_sample->depth_ms;
  }
  if (!g_composite_path_logged && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_composite_pass active=1 "
                 "source=old_architecture_compute_shader target=(%" PRIu32
                 ",%" PRIu32 ") sky_visibility=%.6f ambient_strength=%.6f\n",
                 target_width, target_height,
                 uniforms.sky_visibility_and_ambient_strength[0],
                 uniforms.sky_visibility_and_ambient_strength[1]);
    std::fflush(g_log);
    g_composite_path_logged = true;
  }
  return true;
}

bool present_composite_to_swapchain(SDL_GPUCommandBuffer *command_buffer,
                                    SDL_GPUTexture *composite_texture,
                                    SDL_GPUTexture *swapchain_texture,
                                    const client_shader_pipelines &pipelines,
                                    uint64_t frame_index) {
  if (pipelines.present == nullptr || pipelines.atlas_sampler == nullptr) {
    log_line("live_present_pass active=0 reason=missing_pipeline");
    return false;
  }

  octaryn_client_function_profile_scope present_profile_scope(
      "present_pass", frame_index, "sdl_gpu_commands");
  SDL_GPUColorTargetInfo color_target{};
  color_target.texture = swapchain_texture;
  color_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
  color_target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &color_target, 1u, nullptr);
  if (render_pass == nullptr) {
    log_line("live_present_pass active=0 reason=begin_failed");
    return false;
  }

  SDL_GPUTextureSamplerBinding bindings[2]{};
  bindings[0].texture = composite_texture;
  bindings[0].sampler = pipelines.atlas_sampler;
  bindings[1].texture = composite_texture;
  bindings[1].sampler = pipelines.atlas_sampler;
  const uint32_t overlay_enabled = 0u;
  SDL_BindGPUGraphicsPipeline(render_pass, pipelines.present);
  SDL_BindGPUFragmentSamplers(render_pass, 0u, bindings, 2u);
  SDL_PushGPUFragmentUniformData(command_buffer, 0u, &overlay_enabled,
                                 sizeof(overlay_enabled));
  SDL_DrawGPUPrimitives(render_pass, 3u, 1u, 0u, 0u);
  SDL_EndGPURenderPass(render_pass);

  if (!g_present_path_logged && g_log != nullptr) {
    std::fprintf(g_log,
                 "live_present_pass active=1 source=old_architecture_present_shader "
                 "tone_map=1 linear_to_srgb=1\n");
    std::fflush(g_log);
    g_present_path_logged = true;
  }
  return true;
}

SDL_GPUTexture *create_frame_color_target(SDL_GPUDevice *device,
                                          SDL_GPUTextureFormat format,
                                          uint32_t width, uint32_t height) {
  SDL_GPUTextureCreateInfo texture_info{};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format = format;
  texture_info.usage =
      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
      SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
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

bool present_frame(SDL_GPUDevice *device, SDL_Window *window,
                   const ClientBlockAtlas &atlas,
                   const std::vector<presentation_block> &blocks,
                   const octaryn_client_camera &camera,
                   const client_block_raycast_hit &selection_hit,
                   uint16_t selected_place_block,
                   const client_shader_pipelines &pipelines,
                   const world_mesh_gpu_buffers &mesh_buffers,
                   const world_mesh_upload_frame &mesh_frame,
                   const server_world_time_state &world_time,
                   const octaryn_client_runtime_controls &controls,
                   const octaryn_client_frame_profile_snapshot &profile,
                   uint64_t frame_index,
                   octaryn_client_frame_profile_sample *profile_sample) {
  octaryn_client_function_profile_scope present_frame_profile_scope(
      "present_frame", frame_index, "sdl_gpu");
  const uint64_t render_start = SDL_GetTicksNS();
  const uint64_t command_acquire_start = render_start;
  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (profile_sample != nullptr) {
    profile_sample->command_acquire_ms =
        octaryn_client_frame_profile_elapsed_ms_since(command_acquire_start);
  }
  if (command_buffer == nullptr) {
    log_line("gpu_command_buffer=failed");
    return false;
  }

  SDL_GPUTexture *swapchain_texture = nullptr;
  uint32_t target_width = 0u;
  uint32_t target_height = 0u;
  const uint64_t swapchain_acquire_start = SDL_GetTicksNS();
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                             &swapchain_texture, &target_width,
                                             &target_height) ||
      swapchain_texture == nullptr) {
    log_line("gpu_swapchain_acquire=failed");
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->swapchain_wait_ms =
        octaryn_client_frame_profile_elapsed_ms_since(swapchain_acquire_start);
    profile_sample->swapchain_acquire_ms = profile_sample->swapchain_wait_ms;
    profile_sample->frame_acquire_ms =
        profile_sample->command_acquire_ms + profile_sample->swapchain_wait_ms;
  }

  if (!g_gpu_path_logged && g_log != nullptr) {
    std::fprintf(g_log, "gpu_render_path=SDL_GPU\n");
    std::fprintf(
        g_log, "gpu_swapchain_acquired width=%" PRIu32 " height=%" PRIu32 "\n",
        target_width, target_height);
    std::fflush(g_log);
    g_gpu_path_logged = true;
  }

  const bool validate_pixels = read_enabled_flag(kPixelValidationFlag);
  SDL_GPUTexture *frame_texture = nullptr;
  SDL_GPUTexture *color_texture = nullptr;
  SDL_GPUTexture *depth_texture = nullptr;
  SDL_GPUTexture *position_texture = nullptr;
  SDL_GPUTexture *voxel_texture = nullptr;
  SDL_GPUTexture *material_texture = nullptr;
  auto release_frame_targets = [&]() {
    if (color_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, color_texture);
      color_texture = nullptr;
    }
    if (material_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, material_texture);
      material_texture = nullptr;
    }
    if (voxel_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, voxel_texture);
      voxel_texture = nullptr;
    }
    if (position_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, position_texture);
      position_texture = nullptr;
    }
    if (depth_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, depth_texture);
      depth_texture = nullptr;
    }
  };
  constexpr SDL_GPUTextureFormat color_format =
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  const uint64_t render_setup_start = SDL_GetTicksNS();
  frame_texture = create_composite_frame_texture(
      device, color_format, target_width, target_height);
  if (frame_texture == nullptr) {
    log_line("live_composite_texture=create_failed");
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  color_texture =
      create_frame_color_target(device, color_format, target_width, target_height);
  if (color_texture == nullptr) {
    log_line("gpu_color_texture=create_failed");
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  SDL_GPUTexture *render_texture = color_texture;

  SDL_GPUTextureCreateInfo depth_info{};
  depth_info.type = SDL_GPU_TEXTURETYPE_2D;
  depth_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_info.width = target_width;
  depth_info.height = target_height;
  depth_info.layer_count_or_depth = 1u;
  depth_info.num_levels = 1u;
  depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  depth_texture = SDL_CreateGPUTexture(device, &depth_info);
  if (depth_texture == nullptr) {
    log_line("gpu_depth_texture=create_failed");
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  position_texture = create_frame_color_target(
      device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, target_width,
      target_height);
  voxel_texture = create_frame_color_target(
      device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, target_width,
      target_height);
  material_texture = create_frame_color_target(
      device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, target_width,
      target_height);
  if (position_texture == nullptr || voxel_texture == nullptr ||
      material_texture == nullptr) {
    log_line("gpu_gbuffer_texture=create_failed");
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!clear_gpu_swapchain(command_buffer, render_texture)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->render_setup_ms =
        octaryn_client_frame_profile_elapsed_ms_since(render_setup_start);
  }

  if (!draw_shader_world(command_buffer, render_texture, depth_texture,
                         position_texture, voxel_texture, material_texture,
                         atlas, pipelines, mesh_buffers, mesh_frame, camera,
                         selection_hit, world_time, controls, frame_index,
                         profile_sample)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  int drawn_tiles = 0;
  const bool world_mesh_active = pipelines.world != nullptr &&
                                 mesh_buffers.opaque_faces != nullptr &&
                                 !mesh_frame.opaque_faces.empty();
  if (!world_mesh_active) {
    if (!draw_blocks(command_buffer, render_texture, target_width,
                     target_height, atlas, blocks, camera, drawn_tiles)) {
      release_frame_targets();
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      SDL_CancelGPUCommandBuffer(command_buffer);
      return false;
    }
  }

  if (!draw_material_atlas_probe(command_buffer, render_texture, target_width,
                                 target_height, atlas)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (!run_composite_pass(command_buffer, color_texture, position_texture,
                          voxel_texture, material_texture, frame_texture,
                          pipelines, world_time, camera, controls, target_width,
                          target_height, frame_index, profile_sample)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  const uint64_t ui_start = SDL_GetTicksNS();
  if (!render_ui_overlay(command_buffer, frame_texture, atlas, pipelines,
                         controls, profile, selected_place_block, target_width,
                         target_height)) {
    release_frame_targets();
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->ui_ms =
        octaryn_client_frame_profile_elapsed_ms_since(ui_start);
  }

  const uint64_t blit_start = SDL_GetTicksNS();
  if (!present_composite_to_swapchain(command_buffer, frame_texture,
                                      swapchain_texture, pipelines,
                                      frame_index)) {
    release_frame_targets();
    SDL_ReleaseGPUTexture(device, frame_texture);
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }
  if (profile_sample != nullptr) {
    profile_sample->swapchain_blit_ms =
        octaryn_client_frame_profile_elapsed_ms_since(blit_start);
  }

  gpu_pixel_readback sky_pixel_readback{};
  if (validate_pixels &&
      !begin_sky_pixel_readback(device, command_buffer, frame_texture,
                                color_format, target_width, target_height,
                                sky_pixel_readback)) {
    release_frame_targets();
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    SDL_CancelGPUCommandBuffer(command_buffer);
    return false;
  }

  if (validate_pixels) {
    const uint64_t submit_start = SDL_GetTicksNS();
    SDL_GPUFence *fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms =
          octaryn_client_frame_profile_elapsed_ms_since(submit_start);
    }
    if (fence == nullptr) {
      if (sky_pixel_readback.transfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, sky_pixel_readback.transfer);
      }
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }

    SDL_GPUFence *fences[] = {fence};
    const bool waited = SDL_WaitForGPUFences(device, true, fences, 1u);
    SDL_ReleaseGPUFence(device, fence);
    if (!waited || !finish_sky_pixel_readback(device, sky_pixel_readback)) {
      if (frame_texture != nullptr) {
        SDL_ReleaseGPUTexture(device, frame_texture);
      }
      release_frame_targets();
      return false;
    }
    if (frame_texture != nullptr) {
      SDL_ReleaseGPUTexture(device, frame_texture);
    }
    release_frame_targets();
  } else {
    const uint64_t submit_start = SDL_GetTicksNS();
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
      SDL_ReleaseGPUTexture(device, frame_texture);
      release_frame_targets();
      log_line("gpu_submit=failed");
      return false;
    }
    if (profile_sample != nullptr) {
      profile_sample->render_submit_ms =
          octaryn_client_frame_profile_elapsed_ms_since(submit_start);
    }
    SDL_ReleaseGPUTexture(device, frame_texture);
    release_frame_targets();
  }
  if (profile_sample != nullptr) {
    profile_sample->render_ms =
        octaryn_client_frame_profile_elapsed_ms_since(render_start);
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

} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  open_log();
  octaryn_client_function_profile_configure(g_log);
  octaryn_native_crash_diagnostics_init("octaryn-client-app");
  if (g_log != nullptr) {
    std::fprintf(g_log, "crash_marker=%s\n",
                 octaryn_native_crash_diagnostics_marker_path());
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    log_line("sdl_init=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      close_log();
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
    close_log();
    return 3;
  }

  if (!octaryn_client_window_lifecycle_show(window)) {
    log_line("window_show=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
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
    close_log();
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
    close_log();
    return 7;
  }
  log_line("gpu_device_create=0");
  log_line("gpu_window_claim=0");
  octaryn_client_frame_pacing frame_pacing{};
  octaryn_client_frame_pacing_init(&frame_pacing);
  octaryn_client_swapchain_state swapchain_state{};
  octaryn_client_swapchain_state_init(&swapchain_state);
  if (!octaryn_client_swapchain_configure(&swapchain_state, gpu_device, window,
                                          &frame_pacing)) {
    log_line("gpu_swapchain_configure=failed");
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 7;
  }
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "gpu_swapchain_configure=0 present_mode=%s fps_cap=%d\n",
                 octaryn_client_swapchain_present_mode_name(&swapchain_state),
                 frame_pacing.fps_cap);
    std::fflush(g_log);
  }

  const bool game_modules_disabled = read_enabled_flag(kDisableGameModulesFlag);
  if (g_log != nullptr) {
    std::fprintf(g_log, "client_game_module_load=%s\n",
                 game_modules_disabled ? "disabled" : "enabled");
    std::fflush(g_log);
  }
  singleplayer_server_session server_session{};
  prepare_singleplayer_server_session(server_session, game_modules_disabled);

  ClientBlockAtlas atlas{};
  if (game_modules_disabled) {
    log_line("game_module_descriptor=skipped reason=disabled");
    log_line("block_atlas=skipped reason=disabled");
    if (!load_native_empty_world_atlas(gpu_device, atlas)) {
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 10;
    }
  } else if (!load_bundled_game_module_descriptor() ||
             !load_client_block_atlas(gpu_device, g_log, atlas)) {
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 10;
  }

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;

  int result = octaryn_client_initialize(&api);
  log_result("initialize", result);
  if (result != 0) {
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 5;
  }

  if (read_enabled_flag("OCTARYN_CLIENT_APP_PRESENTATION_PROBE_SNAPSHOT")) {
    result = apply_probe_snapshot();
    log_result("presentation_probe_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_client_block_atlas(atlas);
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 8;
    }
  }

  std::vector<presentation_block> world_snapshot_blocks;
  std::vector<presentation_block> world_surface_blocks;
  server_world_time_state world_time{};
  if (server_session.enabled) {
    log_line("world_blocks_snapshot=deferred source=singleplayer_server");
  } else if (game_modules_disabled) {
    log_line("world_blocks_snapshot=skipped reason=game_modules_disabled");
    log_line("live_chunk_streaming active=1 source=client_native_empty_world");
  } else if (!load_world_snapshot_blocks(world_snapshot_blocks,
                                         world_surface_blocks, world_time)) {
    octaryn_client_shutdown();
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 9;
  }

  if (!world_snapshot_blocks.empty()) {
    result = apply_snapshot_blocks(world_snapshot_blocks, 2u);
    log_result("world_blocks_snapshot", result);
    if (result != 0) {
      octaryn_client_shutdown();
      destroy_client_block_atlas(atlas);
      SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
      SDL_DestroyGPUDevice(gpu_device);
      SDL_DestroyWindow(window);
      SDL_Quit();
      close_log();
      return 10;
    }
  }
  block_lookup world_block_lookup = build_block_lookup(world_snapshot_blocks);
  if (g_log != nullptr) {
    std::fprintf(g_log, "live_block_interaction_lookup blocks=%zu\n",
                 world_block_lookup.size());
    std::fflush(g_log);
  }

  client_shader_pipelines shader_pipelines{};
  if (!initialize_shader_pipelines(gpu_device, window, shader_pipelines)) {
    octaryn_client_shutdown();
    destroy_client_block_atlas(atlas);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_log();
    return 11;
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  octaryn_client_runtime_controls runtime_controls{};
  octaryn_client_runtime_controls_init(&runtime_controls);
  if (octaryn_client_runtime_settings_load(window, &runtime_controls) == 0) {
    log_line("client_settings_load=failed");
  } else {
    log_line("client_settings_load=0");
  }
  octaryn_client_runtime_controls_refresh_menu(&runtime_controls, window,
                                               kWindowWidth, kWindowHeight);
  octaryn_client_runtime_controls_sync_relative_mouse(&runtime_controls,
                                                      window);
  frame_pacing.requested_present_mode =
      runtime_controls.present_mode_index == 0
          ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_IMMEDIATE
          : (runtime_controls.present_mode_index == 1
                 ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_MAILBOX
                 : OCTARYN_CLIENT_PRESENT_MODE_POLICY_VSYNC);
  if (octaryn_client_swapchain_configure(&swapchain_state, gpu_device, window,
                                         &frame_pacing) &&
      g_log != nullptr) {
    std::fprintf(g_log,
                 "gpu_swapchain_configure=0 source=settings present_mode=%s "
                 "fps_cap=%d\n",
                 octaryn_client_swapchain_present_mode_name(&swapchain_state),
                 frame_pacing.fps_cap);
    std::fflush(g_log);
  }
  octaryn_client_frame_metrics frame_metrics{};
  octaryn_client_frame_metrics_init(&frame_metrics);
  octaryn_client_frame_profile_snapshot last_profile{};
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_runtime_controls active=1 f3=1 f11=1 escape_menu=1 "
                 "menu=%u debug=%u render_distance=%d\n",
                 static_cast<unsigned>(runtime_controls.display_menu.active),
                 static_cast<unsigned>(
                     runtime_controls.debug_overlay_enabled),
                 runtime_controls.render_distance);
    std::fflush(g_log);
  }
  octaryn_client_fly_player_controller player{};
  octaryn_client_fly_player_controller_init(&player);
  place_camera_over_snapshot(player.camera, world_surface_blocks);
  octaryn_client_camera_update(&player.camera);
  block_selection_state block_selection{};
  if (!game_modules_disabled) {
    block_selection.selected_block = client_block_atlas_default_placeable_block(
        atlas, kDefaultInteractionPlaceBlock);
  } else {
    block_selection.selected_block = 1u;
  }
  if (g_log != nullptr && !game_modules_disabled) {
    const int32_t layer =
        client_block_atlas_top_layer_for_block(atlas, block_selection.selected_block);
    std::fprintf(g_log,
                 "live_selected_block block=%u layer=%d source=game_module_catalog\n",
                 static_cast<unsigned>(block_selection.selected_block), layer);
    std::fflush(g_log);
  } else if (g_log != nullptr) {
    log_line("live_selected_block active=0 reason=game_modules_disabled");
  }
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
  octaryn_client_chunk_view native_empty_mesh_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  client_key_state keys{};
  client_world_time_controls world_time_controls{};
  octaryn_client_chunk_view logged_chunk_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  server_chunk_stream_file active_server_stream{};
  uint64_t active_server_stream_override_signature =
      std::numeric_limits<uint64_t>::max();
  std::filesystem::file_time_type active_server_stream_write_time{};
  bool loaded_server_world_blocks = false;
  if (server_session.enabled) {
    const octaryn_client_chunk_view initial_chunk_view =
        octaryn_client_chunk_view_for_camera(player.camera.position[0],
                                             player.camera.position[2],
                                             runtime_controls.render_distance);
    octaryn_client_chunk_view empty_previous_view{
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::min(),
        0,
    };
    if (!write_chunk_view_intent(initial_chunk_view, empty_previous_view, 1u)) {
      result = -9;
      running = false;
    } else {
      octaryn_host_frame_snapshot initial_frame =
          create_frame(0u, kDefaultDeltaSeconds);
      client_input_debug_state initial_input{};
      apply_input_to_frame(initial_frame, initial_input, player.camera);
      if (!write_player_input_intent(initial_frame) ||
          !write_world_time_intent(server_session, world_time_controls) ||
          !start_singleplayer_server(server_session)) {
        result = -9;
        running = false;
      }
    }
  }
  while (running) {
    const uint64_t frame_start_ticks = SDL_GetTicksNS();
    octaryn_client_frame_profile_sample profile_sample{};
    const uint64_t misc_start = frame_start_ticks;
    pointer_motion_debug_state pointer_motion{};
    pointer_click_debug_state pointer_click{};
    SDL_Event event{};
    {
      octaryn_client_function_profile_scope profile_scope(
          "event_poll_loop", frame_index + 1u, "");
      while (SDL_PollEvent(&event)) {
      int event_width = 0;
      int event_height = 0;
      window_output_size(window, &event_width, &event_height);
      if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        const bool speed_up =
            event.key.scancode == SDL_SCANCODE_EQUALS ||
            event.key.scancode == SDL_SCANCODE_KP_PLUS;
        const bool slow_down =
            event.key.scancode == SDL_SCANCODE_MINUS ||
            event.key.scancode == SDL_SCANCODE_KP_MINUS;
        if (speed_up || slow_down) {
          const int32_t max_index =
              static_cast<int32_t>(kWorldTimeSpeedMultipliers.size()) - 1;
          world_time_controls.speed_index = clamp_int32(
              world_time_controls.speed_index + (speed_up ? 1 : -1), 0,
              max_index);
          world_time_controls.speed_multiplier =
              kWorldTimeSpeedMultipliers[static_cast<size_t>(
                  world_time_controls.speed_index)];
          world_time_controls.dirty = true;
          if (g_log != nullptr) {
            std::fprintf(g_log,
                         "live_world_time_control speed_index=%d "
                         "speed_multiplier=%.3f\n",
                         world_time_controls.speed_index,
                         world_time_controls.speed_multiplier);
            std::fflush(g_log);
          }
          continue;
        }
      }
      const uint32_t control_result =
          octaryn_client_runtime_controls_handle_event(
              &runtime_controls, window, &event, event_width, event_height);
      if (control_result != 0u && g_log != nullptr) {
        std::fprintf(g_log,
                     "live_runtime_control_event flags=%" PRIu32
                     " debug=%u menu=%u fullscreen=%d render_distance=%d\n",
                     control_result,
                     static_cast<unsigned>(
                         runtime_controls.debug_overlay_enabled),
                     static_cast<unsigned>(
                         runtime_controls.display_menu.active),
                     (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0u
                         ? 1
                         : 0,
                     runtime_controls.render_distance);
        std::fflush(g_log);
      }
      if ((control_result &
           OCTARYN_CLIENT_RUNTIME_CONTROLS_QUIT_REQUESTED) != 0u) {
        running = false;
      }
      if ((control_result &
           OCTARYN_CLIENT_RUNTIME_CONTROLS_MENU_APPLIED) != 0u) {
        const int32_t present_mode_index =
            clamp_int32(runtime_controls.present_mode_index, 0, 2);
        frame_pacing.requested_present_mode =
            present_mode_index == 0
                ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_IMMEDIATE
                : (present_mode_index == 1
                       ? OCTARYN_CLIENT_PRESENT_MODE_POLICY_MAILBOX
                       : OCTARYN_CLIENT_PRESENT_MODE_POLICY_VSYNC);
        if (octaryn_client_swapchain_configure(&swapchain_state, gpu_device,
                                               window, &frame_pacing) &&
            g_log != nullptr) {
          std::fprintf(g_log,
                       "gpu_swapchain_configure=0 source=menu "
                       "present_mode=%s fps_cap=%d\n",
                       octaryn_client_swapchain_present_mode_name(
                           &swapchain_state),
                       frame_pacing.fps_cap);
          std::fflush(g_log);
        }
        if (octaryn_client_runtime_settings_save(window, &runtime_controls) == 0) {
          log_line("client_settings_save=failed");
        } else {
          log_line("client_settings_save=0");
        }
      }
      if ((control_result &
           OCTARYN_CLIENT_RUNTIME_CONTROLS_EVENT_CAPTURED) != 0u) {
        continue;
      }
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
      } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        const float wheel_y = event.wheel.y;
        const int delta = wheel_y > 0.0f ? 1 : (wheel_y < 0.0f ? -1 : 0);
        if (delta != 0 && !game_modules_disabled) {
          block_selection.selected_block =
              client_block_atlas_scroll_placeable_block(
                  atlas, block_selection.selected_block, delta);
          ++block_selection.change_count;
          if (g_log != nullptr) {
            const int32_t layer = client_block_atlas_top_layer_for_block(
                atlas, block_selection.selected_block);
            std::fprintf(
                g_log,
                "live_selected_block block=%u layer=%d wheel_delta=%d "
                "changes=%" PRIu64 "\n",
                static_cast<unsigned>(block_selection.selected_block), layer,
                delta, block_selection.change_count);
            std::fflush(g_log);
          }
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            !SDL_GetWindowRelativeMouseMode(window)) {
          SDL_SetWindowRelativeMouseMode(window, true);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          if (event.button.button == SDL_BUTTON_LEFT) {
            pointer_click.primary = true;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            pointer_click.secondary = true;
          }
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
    }
    profile_sample.misc_ms +=
        octaryn_client_frame_profile_elapsed_ms_since(misc_start);

    const uint64_t current_ticks = SDL_GetTicksNS();
    const uint64_t sim_start = current_ticks;
    double delta_seconds = frame_delta_seconds(previous_ticks, current_ticks);
    if (read_enabled_flag(kInputProbeFlag) && frame_index == 0u) {
      delta_seconds = kDefaultDeltaSeconds;
    }
    octaryn_host_frame_snapshot frame =
        create_frame(frame_index + 1u, delta_seconds);
    client_input_debug_state input =
        read_client_input(window, pointer_motion, pointer_click, keys);
    if (octaryn_client_runtime_controls_ui_active(&runtime_controls) != 0u) {
      input = {};
    }
    apply_input_probe(input, frame.timing.frame_index);
    if (!update_client_player_controller(window, player, input,
                                         frame.timing.delta_seconds)) {
      result = -4;
      running = false;
      break;
    }
    const octaryn_client_camera &camera = player.camera;
    apply_input_to_frame(frame, input, camera);
    const client_block_raycast_hit selection_hit =
        game_modules_disabled
            ? raycast_native_empty_world_interaction(camera, world_block_lookup)
            : raycast_block_interaction(camera, world_block_lookup);
    const octaryn_client_chunk_view chunk_view =
        octaryn_client_chunk_view_for_camera(camera.position[0],
                                             camera.position[2],
                                             runtime_controls.render_distance);
    log_chunk_view_if_changed(frame.timing.frame_index, chunk_view,
                              logged_chunk_view);
    reset_command_frame_counts();
    if (!write_player_input_intent(frame)) {
      result = -7;
      running = false;
      break;
    }
    if (world_time_controls.dirty) {
      if (!write_world_time_intent(server_session, world_time_controls)) {
        result = -9;
        running = false;
        break;
      }
      world_time_controls.dirty = false;
    }
    const bool native_empty_local_edit =
        game_modules_disabled && selection_hit.has_hit &&
        ((input.flags & (kInputPrimaryFlag | kInputSecondaryFlag)) != 0u);
    if (!write_block_interaction_intent(frame, input, camera, selection_hit,
                                        block_selection.selected_block,
                                        world_snapshot_blocks,
                                        world_block_lookup,
                                        game_modules_disabled)) {
      result = -8;
      running = false;
      break;
    }
    bool native_empty_stream_mesh_dirty = false;
    if (server_session.enabled) {
      octaryn_client_function_profile_scope profile_scope(
          "server_stream_poll", frame.timing.frame_index, "");
      if (!loaded_server_world_blocks &&
          std::filesystem::exists(server_session.world_blocks_path)) {
        if (load_world_blocks_from_path(server_session.world_blocks_path,
                                        world_snapshot_blocks,
                                        world_surface_blocks)) {
          loaded_server_world_blocks = true;
          world_block_lookup = build_block_lookup(world_snapshot_blocks);
          place_camera_over_snapshot(player.camera, world_surface_blocks);
          octaryn_client_camera_update(&player.camera);
          result = apply_snapshot_blocks(world_snapshot_blocks,
                                         frame.timing.frame_index + 9u);
          log_result("server_world_blocks_snapshot", result);
          if (result != 0) {
            running = false;
            break;
          }
        }
      }
      std::error_code stream_time_error;
      const auto stream_write_time = std::filesystem::last_write_time(
          server_session.chunk_stream_path, stream_time_error);
      if (!stream_time_error &&
          stream_write_time != active_server_stream_write_time) {
        server_chunk_stream_file loaded_stream{};
        if (!load_server_chunk_stream_file(loaded_stream, world_time, true)) {
          result = -9;
          running = false;
          break;
        }

        active_server_stream_write_time = stream_write_time;
        if (game_modules_disabled) {
          const octaryn_client_chunk_view loaded_stream_view =
              chunk_view_from_server_stream(loaded_stream);
          const bool stream_view_changed =
              !same_chunk_view(native_empty_mesh_chunk_view,
                               loaded_stream_view);
          const uint64_t loaded_override_signature =
              hash_world_block_records(loaded_stream.blocks);
          const bool override_records_changed =
              loaded_override_signature != active_server_stream_override_signature;

          active_server_stream = std::move(loaded_stream);
          if (override_records_changed) {
            apply_native_empty_overrides_from_records(active_server_stream.blocks,
                                                      world_block_lookup);
            active_server_stream_override_signature =
                loaded_override_signature;
          }
          native_empty_stream_mesh_dirty =
              stream_view_changed || override_records_changed;
          if (!native_empty_stream_mesh_dirty && g_log != nullptr) {
            std::fprintf(
                g_log,
                "native_empty_chunk_stream active=1 source=server_background "
                "rebuild=0 reason=time_only_stream epoch=%" PRIu64
                " render_distance=%" PRIu32
                " columns=%zu override_edits=%zu "
                "world_time_day_fraction=%.6f\n",
                active_server_stream.epoch, active_server_stream.radius,
                active_server_stream.columns.size(),
                world_block_lookup.size(), world_time.day_fraction);
            std::fflush(g_log);
          }
        } else if (!loaded_stream.blocks.empty()) {
          active_server_stream = std::move(loaded_stream);
          apply_blocks_from_records(active_server_stream.blocks, false,
                                    world_snapshot_blocks);
          apply_top_blocks_from_records(active_server_stream.blocks, false,
                                        world_surface_blocks);
          world_block_lookup = build_block_lookup(world_snapshot_blocks);
          if (!world_snapshot_blocks.empty()) {
            result = apply_snapshot_blocks(world_snapshot_blocks,
                                           frame.timing.frame_index + 10u);
            log_result("server_chunk_stream_snapshot", result);
            if (result != 0) {
              running = false;
              break;
            }
          }
        } else {
          active_server_stream = std::move(loaded_stream);
        }
      }
    }
    previous_ticks = current_ticks;
    log_client_tick_input_frame(frame);
    profile_sample.sim_ms =
        octaryn_client_frame_profile_elapsed_ms_since(sim_start);

    const uint64_t world_start = SDL_GetTicksNS();
    result = octaryn_client_tick(&frame);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    if (game_modules_disabled) {
      if (server_session.enabled &&
          (native_empty_stream_mesh_dirty || native_empty_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        octaryn_client_function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "server_background");
        const octaryn_client_chunk_view mesh_chunk_view =
            !active_server_stream.columns.empty()
                ? chunk_view_from_server_stream(active_server_stream)
                : chunk_view;
        if (!active_server_stream.columns.empty()) {
          build_native_empty_world_mesh_frame_from_stream(
              active_server_stream, world_block_lookup,
              native_empty_mesh_chunk_view, mesh_upload_frame);
        } else {
          build_native_empty_world_mesh_frame(
              chunk_view, native_empty_mesh_chunk_view, world_block_lookup,
              mesh_upload_frame);
        }
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        native_empty_mesh_chunk_view = mesh_chunk_view;
        if (visible_world_mesh_frame.chunks.empty()) {
          release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
        } else {
          octaryn_client_function_profile_scope upload_profile_scope(
              "world_mesh_upload", frame.timing.frame_index,
              "native_empty_server");
          if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers,
                                       frame.timing.frame_index)) {
            result = -6;
            running = false;
            break;
          }
        }
      } else if (!server_session.enabled &&
                 (!same_chunk_view(native_empty_mesh_chunk_view, chunk_view) ||
                  native_empty_local_edit)) {
        world_mesh_upload_frame mesh_upload_frame{};
        octaryn_client_function_profile_scope mesh_profile_scope(
            "native_empty_mesh_build", frame.timing.frame_index,
            "client_native");
        build_native_empty_world_mesh_frame(
            chunk_view, native_empty_mesh_chunk_view, world_block_lookup,
            mesh_upload_frame);
        visible_world_mesh_frame = std::move(mesh_upload_frame);
        native_empty_mesh_chunk_view = chunk_view;
        if (visible_world_mesh_frame.chunks.empty()) {
          release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
        } else {
          octaryn_client_function_profile_scope upload_profile_scope(
              "world_mesh_upload", frame.timing.frame_index,
              "native_empty_client");
          if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                       mesh_buffers,
                                       frame.timing.frame_index)) {
            result = -6;
            running = false;
            break;
          }
        }
      }
    } else {
      world_mesh_upload_frame mesh_upload_frame{};
      if (!drain_chunk_mesh_uploads(frame.timing.frame_index,
                                    mesh_upload_scratch, mesh_upload_frame)) {
        result = -5;
        running = false;
        break;
      }
      if (!mesh_upload_frame.chunks.empty()) {
        merge_world_mesh_upload_frame(visible_world_mesh_frame,
                                      mesh_upload_frame,
                                      frame.timing.frame_index);
        octaryn_client_function_profile_scope upload_profile_scope(
            "world_mesh_upload", frame.timing.frame_index, "game_module");
        if (!upload_world_mesh_frame(gpu_device, visible_world_mesh_frame,
                                     mesh_buffers, frame.timing.frame_index)) {
          result = -6;
          running = false;
          break;
        }
      }
    }
    const bool world_mesh_active = mesh_buffers.opaque_faces != nullptr &&
                                   !visible_world_mesh_frame.opaque_faces.empty();
    uint32_t drained_updates = 0u;
    if (!world_mesh_active &&
        !drain_presentation_updates(presentation_blocks, drained_updates)) {
      result = -3;
      running = false;
      break;
    }
    log_live_client_frame(frame.timing.frame_index, input,
                          command_frame_counts(), camera, drained_updates,
                          presentation_blocks);
    profile_sample.world_ms =
        octaryn_client_frame_profile_elapsed_ms_since(world_start);

    if (!present_frame(gpu_device, window, atlas, presentation_blocks, camera,
                       selection_hit, block_selection.selected_block,
                       shader_pipelines, mesh_buffers, visible_world_mesh_frame,
                       world_time, runtime_controls, last_profile,
                       frame.timing.frame_index,
                       &profile_sample)) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      }
      result = -2;
      running = false;
      break;
    }
    const uint64_t frame_end_ticks = SDL_GetTicksNS();
    profile_sample.total_ms =
        octaryn_client_frame_profile_elapsed_ms(frame_start_ticks,
                                                frame_end_ticks);
    octaryn_client_frame_profile_finalize_sample(&profile_sample);
    octaryn_client_frame_metrics_record(&frame_metrics, profile_sample.total_ms,
                                        frame_end_ticks);
    last_profile.sample = profile_sample;
    last_profile.metrics =
        octaryn_client_frame_metrics_snapshot_value(&frame_metrics,
                                                    frame_end_ticks);
    log_frame_profile(frame.timing.frame_index, last_profile,
                      runtime_controls.debug_overlay_enabled);

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }
  }

  stop_singleplayer_server(server_session);
  octaryn_client_shutdown();
  log_line("shutdown=0");
  release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
  release_shader_pipelines(gpu_device, shader_pipelines);
  destroy_client_block_atlas(atlas);
  SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
  SDL_DestroyGPUDevice(gpu_device);
  SDL_DestroyWindow(window);
  SDL_Quit();

  close_log();

  return result == 0 ? 0 : 6;
}
