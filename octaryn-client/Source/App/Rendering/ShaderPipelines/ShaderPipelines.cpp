#include "ShaderPipelines.h"

#include "Environment.h"
#include "FileIO.h"
#include "JsonContracts.h"
#include "Log.h"
#include "ShaderCreation.h"

#include <glaze/glaze.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace octaryn_client_app {

namespace {

using octaryn::client::rendering::ComputeShaderMetadata;
using octaryn::client::rendering::create_compute_pipeline;
using octaryn::client::rendering::create_graphics_shader;
using octaryn::client::rendering::GraphicsShaderMetadata;

constexpr glz::opts kJsonReadOptions{.error_on_unknown_keys = false};

struct compiled_graphics_shader {
  GraphicsShaderMetadata metadata{};
  std::vector<uint8_t> code;
};

struct compiled_compute_shader {
  ComputeShaderMetadata metadata{};
  std::vector<uint8_t> code;
};

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
  if (!read_text_file(metadata_path,
                      "client_compute_shader_metadata=open_failed",
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

SDL_GPUComputePipeline *
create_compute_shader_pipeline(SDL_GPUDevice *device, const char *shader_name) {
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
    SDL_GPUCullMode cull_mode, const char *vertex_shader_name,
    const char *fragment_shader_name) {
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
  info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
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

} // namespace

bool initialize_shader_pipelines(SDL_GPUDevice *device, SDL_Window *window,
                                 client_shader_pipelines &pipelines) {
  const SDL_GPUTextureFormat swapchain_format =
      SDL_GetGPUSwapchainTextureFormat(device, window);
  constexpr SDL_GPUTextureFormat color_format =
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  constexpr SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  pipelines.sky =
      create_swapchain_pipeline(device, color_format, depth_format, false,
                                SDL_GPU_CULLMODE_NONE, "sky.vert", "sky.frag");
  pipelines.world = create_swapchain_pipeline(
      device, color_format, depth_format, true, SDL_GPU_CULLMODE_NONE,
      "opaque_packed.vert", "opaque.frag");
  pipelines.transparent = create_swapchain_pipeline(
      device, color_format, depth_format, true, SDL_GPU_CULLMODE_NONE,
      "transparent_packed.vert", "transparent.frag");
  pipelines.opaque_sprite = create_swapchain_pipeline(
      device, color_format, depth_format, true, SDL_GPU_CULLMODE_NONE,
      "sprite_packed.vert", "opaque.frag");
  pipelines.player_model = create_swapchain_pipeline(
      device, color_format, depth_format, true, SDL_GPU_CULLMODE_BACK,
      "player_model.vert", "player_model.frag");
  pipelines.present = create_swapchain_pipeline(
      device, swapchain_format, depth_format, false, SDL_GPU_CULLMODE_NONE,
      "present.vert", "present.frag");
  pipelines.composite =
      create_compute_shader_pipeline(device, "composite.comp");
  pipelines.ui = create_compute_shader_pipeline(device, "ui.comp");

  SDL_GPUSamplerCreateInfo sampler_info{};
  sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.enable_anisotropy = false;
  sampler_info.max_anisotropy = 1.0f;
  sampler_info.max_lod = 16.0f;
  pipelines.atlas_sampler = SDL_CreateGPUSampler(device, &sampler_info);
  if (pipelines.atlas_sampler == nullptr) {
    pipelines.atlas_sampler = SDL_CreateGPUSampler(device, &sampler_info);
  }

  SDL_GPUSamplerCreateInfo nearest_info{};
  nearest_info.min_filter = SDL_GPU_FILTER_NEAREST;
  nearest_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  nearest_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  nearest_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  nearest_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  nearest_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  nearest_info.max_lod = 0.0f;
  pipelines.nearest_sampler = SDL_CreateGPUSampler(device, &nearest_info);

  if (pipelines.sky == nullptr || pipelines.world == nullptr ||
      pipelines.transparent == nullptr ||
      pipelines.opaque_sprite == nullptr || pipelines.player_model == nullptr ||
      pipelines.present == nullptr ||
      pipelines.composite == nullptr || pipelines.ui == nullptr ||
      pipelines.atlas_sampler == nullptr || pipelines.nearest_sampler == nullptr) {
    log_line("live_shader_pipeline active=0 reason=create_failed");
    return false;
  }

  log_line(
      "live_shader_pipeline active=1 sky=1 world=1 transparent=1 opaque_sprite=1 present=1 "
      "player_model=1 composite=1 ui=1 block_highlight=texture atlas_mip_sampler=nearest_linear_mip "
      "nearest_sampler=1 source=compiled_spirv");
  return true;
}

void release_shader_pipelines(SDL_GPUDevice *device,
                              client_shader_pipelines &pipelines) {
  if (pipelines.nearest_sampler != nullptr) {
    SDL_ReleaseGPUSampler(device, pipelines.nearest_sampler);
    pipelines.nearest_sampler = nullptr;
  }
  if (pipelines.atlas_sampler != nullptr) {
    SDL_ReleaseGPUSampler(device, pipelines.atlas_sampler);
    pipelines.atlas_sampler = nullptr;
  }
  if (pipelines.world != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.world);
    pipelines.world = nullptr;
  }
  if (pipelines.transparent != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.transparent);
    pipelines.transparent = nullptr;
  }
  if (pipelines.opaque_sprite != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.opaque_sprite);
    pipelines.opaque_sprite = nullptr;
  }
  if (pipelines.player_model != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipelines.player_model);
    pipelines.player_model = nullptr;
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

} // namespace octaryn_client_app
