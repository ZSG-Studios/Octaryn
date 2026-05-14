find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(OCTARYN_ATLAS_TILE_SIZE
    "32"
    CACHE STRING
    "Square tile size, in pixels, used for generated block atlas layers")
set(OCTARYN_ATLAS_LAYER_COUNT
    "32"
    CACHE STRING
    "Number of block atlas layers generated for the runtime texture array")
set(OCTARYN_ATLAS_ALPHA_CUTOFF
    "0.35"
    CACHE STRING
    "Alpha cutoff shared by generated atlas mips and cutout shaders")
set(OCTARYN_ATLAS_WATER_STILL_LAYER
    "16"
    CACHE STRING
    "Atlas layer used for still water surfaces")
set(OCTARYN_ATLAS_WATER_FLOW_LAYER
    "26"
    CACHE STRING
    "Atlas layer used for flowing water surfaces")
set(OCTARYN_ATLAS_LAVA_STILL_LAYER
    "27"
    CACHE STRING
    "Atlas layer used for still lava surfaces")
set(OCTARYN_ATLAS_LAVA_FLOW_LAYER
    "28"
    CACHE STRING
    "Atlas layer used for flowing lava surfaces")
if(NOT OCTARYN_ATLAS_TILE_SIZE MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "OCTARYN_ATLAS_TILE_SIZE must be a positive integer")
endif()
if(NOT OCTARYN_ATLAS_LAYER_COUNT MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "OCTARYN_ATLAS_LAYER_COUNT must be a positive integer")
endif()
if(OCTARYN_ATLAS_ALPHA_CUTOFF LESS 0.0 OR OCTARYN_ATLAS_ALPHA_CUTOFF GREATER 1.0)
  message(FATAL_ERROR "OCTARYN_ATLAS_ALPHA_CUTOFF must be between 0 and 1")
endif()
if(NOT OCTARYN_ATLAS_WATER_STILL_LAYER MATCHES "^[0-9]+$")
  message(FATAL_ERROR "OCTARYN_ATLAS_WATER_STILL_LAYER must be a non-negative integer")
endif()
if(NOT OCTARYN_ATLAS_WATER_FLOW_LAYER MATCHES "^[0-9]+$")
  message(FATAL_ERROR "OCTARYN_ATLAS_WATER_FLOW_LAYER must be a non-negative integer")
endif()
if(NOT OCTARYN_ATLAS_LAVA_STILL_LAYER MATCHES "^[0-9]+$")
  message(FATAL_ERROR "OCTARYN_ATLAS_LAVA_STILL_LAYER must be a non-negative integer")
endif()
if(NOT OCTARYN_ATLAS_LAVA_FLOW_LAYER MATCHES "^[0-9]+$")
  message(FATAL_ERROR "OCTARYN_ATLAS_LAVA_FLOW_LAYER must be a non-negative integer")
endif()

set(OCTARYN_TEXTURE_PACK_URL
    "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/archive/refs/heads/java-latest.zip"
    CACHE STRING
    "Minecraft resource-pack zip URL used to generate the runtime atlas")
set(OCTARYN_TEXTURE_PACK_PATH
    ""
    CACHE FILEPATH
    "Local Minecraft resource-pack zip used instead of downloading OCTARYN_TEXTURE_PACK_URL")
set(OCTARYN_TEXTURE_PACK_CACHE_DIR
    "${OCTARYN_ENGINE_ROOT_DIR}/.octaryn-cache/texture-packs"
    CACHE PATH
    "Local cache directory for downloaded texture packs")
set(OCTARYN_TEXTURE_PACK_SHA256
    ""
    CACHE STRING
    "Optional SHA256 expected for the selected texture-pack zip")

set(octaryn_engine_generated_texture_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_assets/textures")
set(octaryn_engine_generated_include_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_include")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/render/atlas/config.h.in"
  "${octaryn_engine_generated_include_dir}/render/atlas/config.h"
  @ONLY)
set(octaryn_engine_generated_atlas "${octaryn_engine_generated_texture_dir}/atlas.png")
set(octaryn_engine_generated_normal_atlas "${octaryn_engine_generated_texture_dir}/atlas_n.png")
set(octaryn_engine_generated_specular_atlas "${octaryn_engine_generated_texture_dir}/atlas_s.png")
set(octaryn_engine_generated_atlas_manifest "${octaryn_engine_generated_texture_dir}/atlas.txt")
set(octaryn_engine_generated_animation_atlas "${octaryn_engine_generated_texture_dir}/atlas_anim.png")
set(octaryn_engine_generated_animation_manifest "${octaryn_engine_generated_texture_dir}/atlas_anim.txt")
set(octaryn_engine_generated_texture_outputs
  "${octaryn_engine_generated_atlas}"
  "${octaryn_engine_generated_normal_atlas}"
  "${octaryn_engine_generated_specular_atlas}"
  "${octaryn_engine_generated_atlas_manifest}"
  "${octaryn_engine_generated_animation_atlas}"
  "${octaryn_engine_generated_animation_manifest}")
set(octaryn_engine_atlas_tool "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Tools/build_atlas_from_pack.py")
set(octaryn_engine_atlas_pack_args)
if(OCTARYN_TEXTURE_PACK_PATH)
  list(APPEND octaryn_engine_atlas_pack_args --pack "${OCTARYN_TEXTURE_PACK_PATH}")
endif()
if(OCTARYN_TEXTURE_PACK_SHA256)
  list(APPEND octaryn_engine_atlas_pack_args --sha256 "${OCTARYN_TEXTURE_PACK_SHA256}")
endif()

add_custom_command(
  OUTPUT ${octaryn_engine_generated_texture_outputs}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${octaryn_engine_generated_texture_dir}"
  COMMAND Python3::Interpreter
    "${octaryn_engine_atlas_tool}"
    --url "${OCTARYN_TEXTURE_PACK_URL}"
    --cache-dir "${OCTARYN_TEXTURE_PACK_CACHE_DIR}"
    --output "${octaryn_engine_generated_atlas}"
    --normal-output "${octaryn_engine_generated_normal_atlas}"
    --specular-output "${octaryn_engine_generated_specular_atlas}"
    --animation-output "${octaryn_engine_generated_animation_atlas}"
    --animation-manifest "${octaryn_engine_generated_animation_manifest}"
    --tile-size "${OCTARYN_ATLAS_TILE_SIZE}"
    --layer-count "${OCTARYN_ATLAS_LAYER_COUNT}"
    ${octaryn_engine_atlas_pack_args}
  DEPENDS "${octaryn_engine_atlas_tool}"
  VERBATIM)

add_custom_target(octaryn_engine_texture_atlas DEPENDS ${octaryn_engine_generated_texture_outputs})

add_library(octaryn_engine_log STATIC
  core/log.cpp)

target_link_libraries(octaryn_engine_log
  PRIVATE
    octaryn::deps::spdlog)

target_include_directories(octaryn_engine_log
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_features(octaryn_engine_log PRIVATE cxx_std_23)
octaryn_enable_default_warnings(octaryn_engine_log)

add_library(octaryn_engine_diagnostics STATIC
  core/crash_diagnostics.cpp)

target_include_directories(octaryn_engine_diagnostics
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(octaryn_engine_diagnostics
  PRIVATE
    octaryn_engine_log)

if(TARGET octaryn::deps::cpptrace)
  target_link_libraries(octaryn_engine_diagnostics PRIVATE octaryn::deps::cpptrace)
endif()

target_compile_features(octaryn_engine_diagnostics PRIVATE cxx_std_23)
octaryn_enable_default_warnings(octaryn_engine_diagnostics)

add_library(octaryn_engine_memory STATIC
  core/memory_mimalloc.cpp)

target_include_directories(octaryn_engine_memory
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(octaryn_engine_memory
  PRIVATE
    octaryn_engine_log
    octaryn::deps::sdl3
    octaryn::deps::mimalloc)

target_compile_features(octaryn_engine_memory PRIVATE cxx_std_23)
octaryn_enable_default_warnings(octaryn_engine_memory)

if(NOT TARGET imgui)
  octaryn_add_imgui_dependency()
endif()
if(NOT TARGET implot OR NOT TARGET implot3d OR NOT TARGET imgui_node_editor OR NOT TARGET imguizmo OR NOT TARGET imanim OR NOT TARGET imfiledialog)
  octaryn_add_imgui_extension_dependencies()
endif()
if(OCTARYN_ENABLE_UI_TEXT AND NOT TARGET SDL3_ttf::SDL3_ttf)
  octaryn_add_ui_dependencies()
endif()

add_library(octaryn_engine_imgui_backend STATIC
  "${OCTARYN_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${OCTARYN_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp")

target_link_libraries(octaryn_engine_imgui_backend
  PRIVATE
    octaryn::deps::sdl3
    octaryn::deps::imgui)

target_compile_features(octaryn_engine_imgui_backend PRIVATE cxx_std_23)

add_executable(octaryn_engine_runtime
  core/asset_paths.cpp
  core/world_time/calendar.cpp
  core/world_time/snapshot.cpp
  core/world_time/state.cpp
  app/app_settings.cpp
  app/main.cpp
  app/audio/audio.cpp
  app/audio/lifecycle.cpp
  app/audio/play.cpp
  app/audio/synthesis.cpp
  app/display/menu.cpp
  app/display/commit.cpp
  app/display/discovery.cpp
  app/display/state.cpp
  app/lighting/settings/settings.cpp
  app/lighting/settings/defaults.cpp
  app/lighting/settings/storage.cpp
  app/lighting/ui/imgui.cpp
  app/lighting/ui/panel.cpp
  app/managed/managed_command_queue.cpp
  app/managed/managed_frame_snapshot.cpp
  app/managed/managed_host.cpp
  app/overlay/display_menu.cpp
  app/overlay/interaction.cpp
  app/player/player.cpp
  app/player/blocks.cpp
  app/player/spawn.cpp
  app/player/physics/collision.cpp
  app/player/physics/movement.cpp
  app/player/physics/physics.cpp
  app/runtime/diagnostics.cpp
  app/runtime/frame_metrics.cpp
  app/runtime/events.cpp
  app/runtime/frame_pacing.cpp
  app/runtime/frame_profile.cpp
  app/runtime/iterate.cpp
  app/runtime/launch.cpp
  app/runtime/lifecycle.cpp
  app/runtime/renderdoc_capture.cpp
  app/runtime/options.cpp
  app/runtime/settings.cpp
  app/runtime/shutdown.cpp
  app/runtime/stream_benchmark.cpp
  app/runtime/telemetry.cpp
  app/runtime/render/begin.cpp
  app/runtime/render/context.cpp
  app/runtime/render/passes.cpp
  app/runtime/render/present.cpp
  app/runtime/render/probe.cpp
  app/runtime/render/render.cpp
  app/runtime/render/scene.cpp
  app/runtime/render/submit.cpp
  app/runtime/startup/env.cpp
  app/runtime/startup/platform.cpp
  app/runtime/startup/process.cpp
  app/runtime/startup/runtime.cpp
  app/runtime/startup/session.cpp
  app/runtime/startup/startup.cpp
  physics/jolt/jolt_layers.cpp
  physics/jolt/jolt_physics_service.cpp
  physics/jolt/jolt_trace.cpp
  physics/jolt/jolt_worker_jobs.cpp
  app/startup_terrain/debug.cpp
  app/startup_terrain/render_distance.cpp
  app/startup_terrain/spawn.cpp
  app/startup_terrain/terrain.cpp
  app/window/window.cpp
  app/window/display.cpp
  app/window/swapchain.cpp
  app/window/title.cpp
  app/world_time/clock.cpp
  runtime/jobs/runtime_worker_policy.cpp
  runtime/jobs/runtime_worker_pool.cpp
  world/block/block.cpp
  world/block/classification.cpp
  world/block/definition.cpp
  world/block/skylight.cpp
  world/block/water.cpp
  render/buffer/buffer.cpp
  render/buffer/gpu.cpp
  core/camera/camera.cpp
  core/camera/matrix.cpp
  render/atlas/atlas.cpp
  render/atlas/animation.cpp
  render/atlas/destroy.cpp
  render/atlas/icon.cpp
  render/atlas/samplers.cpp
  render/atlas/texture.cpp
  render/atlas/upload/alpha.cpp
  render/atlas/upload/copy.cpp
  render/atlas/upload/data.cpp
  render/atlas/upload/filter.cpp
  render/atlas/upload/mips.cpp
  render/atlas/upload/pack.cpp
  render/atlas/upload/pixels.cpp
  render/atlas/upload/transfer.cpp
  render/atlas/upload/upload.cpp
  render/pipelines/compute.cpp
  render/pipelines/entrypoints.cpp
  render/pipelines/graphics.cpp
  render/pipelines/graphics_mesh.cpp
  render/pipelines/graphics_screen.cpp
  render/pipelines/init.cpp
  render/pipelines/pipelines.cpp
  render/pipelines/release.cpp
  render/scene/forward.cpp
  render/scene/gbuffer.cpp
  render/scene/passes.cpp
  render/visibility/flags.cpp
  render/postprocess/postprocess.cpp
  render/postprocess/composite.cpp
  render/postprocess/composite_setup.cpp
  render/postprocess/depth.cpp
  render/resources/resources.cpp
  render/resources/window_textures.cpp
  render/ui/ui.cpp
  render/world/debug.cpp
  render/world/render.cpp
  core/persistence/cache.cpp
  core/persistence/chunk_files.cpp
  core/persistence/codec.cpp
  core/persistence/io.cpp
  core/persistence/lighting_tuning.cpp
  core/persistence/persistence.cpp
  core/persistence/player.cpp
  core/persistence/settings.cpp
  core/persistence/paths.cpp
  core/persistence/serialization.cpp
  core/persistence/world_save.cpp
  core/persistence/world_snapshot.cpp
  core/persistence/world.cpp
  core/persistence/world_time_store.cpp
  render/shader/create.cpp
  render/shader/shader.cpp
  render/shader/metadata.cpp
  render/world/voxel.cpp
  world/lighting/skylight_cpu.cpp
  world/chunks/chunk.cpp
  world/chunks/edit.cpp
  world/chunks/build_light.cpp
  world/chunks/build_mesh.cpp
  world/chunks/query.cpp
  world/chunks/status.cpp
  world/chunks/build.cpp
  world/chunks/snapshot.cpp
  world/chunks/runtime.cpp
  world/chunks/generation.cpp
  world/chunks/generation_blocks.cpp
  world/chunks/upload_telemetry.cpp
  world/chunks/upload_direct.cpp
  world/chunks/upload_light.cpp
  world/chunks/upload_mesh.cpp
  world/chunks/upload.cpp
  world/chunks/lifecycle.cpp
  world/chunks/state.cpp
  world/edit/bookkeeping.cpp
  world/edit/hidden_blocks.cpp
  world/edit/traces.cpp
  world/edit/environment.cpp
  world/edit/schedule.cpp
  world/edit/light_dirty.cpp
  world/edit/water.cpp
  world/edit/apply.cpp
  world/edit/edit.cpp
  world/edit/queue.cpp
  world/runtime/inspect.cpp
  world/runtime/surface_query.cpp
  world/runtime/render_descriptors.cpp
  world/runtime/update_telemetry.cpp
  world/jobs/jobs.cpp
  world/jobs/scheduling.cpp
  world/jobs/taskflow/backend.cpp
  world/jobs/taskflow/execution.cpp
  world/jobs/taskflow/scheduler.cpp
  world/jobs/taskflow/submission.cpp
  world/jobs/taskflow/slot_pool.cpp
  world/jobs/taskflow/worker_queue.cpp
  world/jobs/taskflow/worker_policy.cpp
  world/runtime/runtime.cpp
  world/runtime/indices.cpp
  world/runtime/state.cpp
  world/window/window.cpp
  world/window/move.cpp
  world/window/update.cpp
  world/generation/features.cpp
  world/generation/noise.cpp
  world/generation/terrain.cpp
  world/generation/worldgen.cpp
  world/runtime/world.cpp
  world/runtime/query.cpp
)

if(OCTARYN_ENABLE_RUNTIME_ASSET_GENERATION)
  set(octaryn_engine_shader_sources
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/composite.comp.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/clouds.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/depth.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/depth_packed.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/depth.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/opaque.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/opaque_packed.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/opaque.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/present.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/present.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/selection.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/selection.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/sky.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/sky.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/sprite_packed.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/transparent.frag.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/transparent_packed.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/transparent.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/water.vert.glsl"
    "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/ui.comp.glsl")

  add_executable(octaryn_engine_shader_tool
    "${OCTARYN_ENGINE_ROOT_DIR}/source/tools/shader_tool_main.cpp")

  target_link_libraries(octaryn_engine_shader_tool
    PRIVATE
      octaryn_engine_log
      octaryn_engine_diagnostics
      octaryn_engine_memory
      octaryn::deps::sdl3
      octaryn::deps::shadercross
      octaryn::deps::shaderc
      octaryn::deps::glaze
      octaryn::deps::spirv_tools
      octaryn::deps::spirv_cross
      octaryn::deps::taskflow)

  target_compile_features(octaryn_engine_shader_tool PRIVATE cxx_std_23)
  set_target_properties(octaryn_engine_shader_tool PROPERTIES OUTPUT_NAME octaryn-engine-shader-tool)
  octaryn_enable_default_warnings(octaryn_engine_shader_tool)
  octaryn_apply_build_layout(octaryn_engine_shader_tool PRODUCT octaryn-engine)

  set(octaryn_engine_shader_outputs)
  set(octaryn_engine_shader_tool_inputs)
  foreach(shader_source IN LISTS octaryn_engine_shader_sources)
    if(NOT shader_source MATCHES "\\.glsl$")
      message(FATAL_ERROR "Runtime shader assets must use explicit GLSL sources: ${shader_source}")
    endif()
    get_filename_component(shader_name "${shader_source}" NAME)
    string(REGEX REPLACE "\\.glsl$" "" shader_output_name "${shader_name}")
    list(APPEND octaryn_engine_shader_tool_inputs --input "${shader_source}")
    list(APPEND octaryn_engine_shader_outputs
      "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/${shader_output_name}.spv"
      "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/${shader_output_name}.json"
      "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/${shader_output_name}.msl")
  endforeach()

  add_custom_command(
    OUTPUT ${octaryn_engine_shader_outputs}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
    COMMAND $<TARGET_FILE:octaryn_engine_shader_tool> --output-dir "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders" ${octaryn_engine_shader_tool_inputs}
    DEPENDS octaryn_engine_shader_tool ${octaryn_engine_shader_sources}
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/atlas_config.glsl"
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/shader_common.glsl"
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/pbr_material.glsl"
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/tiled_atlas_sampling.glsl"
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/voxel_common.glsl"
      "${OCTARYN_ENGINE_ROOT_DIR}/source/shaders/voxel.inc"
    COMMAND_EXPAND_LISTS
    VERBATIM)

  add_custom_target(octaryn_engine_shader_assets DEPENDS ${octaryn_engine_shader_outputs})
  add_dependencies(octaryn_engine_runtime octaryn_engine_shader_assets)
endif()

target_include_directories(octaryn_engine_runtime
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${octaryn_engine_generated_include_dir}"
    "${OCTARYN_IMGUI_SOURCE_DIR}"
    "${OCTARYN_IMGUI_SOURCE_DIR}/backends")

if(DEFINED CPM_PACKAGE_Taskflow_SOURCE_DIR AND EXISTS "${CPM_PACKAGE_Taskflow_SOURCE_DIR}")
  target_include_directories(octaryn_engine_runtime SYSTEM PRIVATE "${CPM_PACKAGE_Taskflow_SOURCE_DIR}")
endif()

if(OCTARYN_ENABLE_MANAGED_DOTNET)
  find_program(OCTARYN_DOTNET_EXECUTABLE dotnet REQUIRED)
  set(octaryn_default_dotnet_root "/usr/share/dotnet")
  if(DEFINED ENV{OCTARYN_DOTNET_ROOT})
    set(octaryn_default_dotnet_root "$ENV{OCTARYN_DOTNET_ROOT}")
  elseif(DEFINED ENV{DOTNET_ROOT})
    set(octaryn_default_dotnet_root "$ENV{DOTNET_ROOT}")
  endif()
  set(OCTARYN_DOTNET_ROOT "${octaryn_default_dotnet_root}" CACHE PATH "Root directory of the .NET installation")
  file(GLOB octaryn_dotnet_native_host_dirs CONFIGURE_DEPENDS
    "${OCTARYN_DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.*/runtimes/*/native"
    "${OCTARYN_DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.*/*/runtimes/*/native")
  find_path(OCTARYN_DOTNET_HOST_INCLUDE_DIR
    NAMES nethost.h hostfxr.h coreclr_delegates.h
    PATHS ${octaryn_dotnet_native_host_dirs}
    NO_DEFAULT_PATH)
  find_library(OCTARYN_DOTNET_NETHOST_LIBRARY
    NAMES nethost
    PATHS ${octaryn_dotnet_native_host_dirs}
    NO_DEFAULT_PATH)
  if(NOT OCTARYN_DOTNET_HOST_INCLUDE_DIR OR NOT OCTARYN_DOTNET_NETHOST_LIBRARY)
    message(FATAL_ERROR "Could not find .NET native host headers/library under ${OCTARYN_DOTNET_ROOT}")
  endif()

  target_include_directories(octaryn_engine_runtime
    SYSTEM PRIVATE
      "${OCTARYN_DOTNET_HOST_INCLUDE_DIR}")

  target_compile_definitions(octaryn_engine_runtime
    PRIVATE
      OCTARYN_ENABLE_MANAGED_DOTNET=1)
endif()

if(TARGET octaryn_engine_shader_tool)
  target_include_directories(octaryn_engine_shader_tool
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}")

  if(DEFINED CPM_PACKAGE_Taskflow_SOURCE_DIR AND EXISTS "${CPM_PACKAGE_Taskflow_SOURCE_DIR}")
    target_include_directories(octaryn_engine_shader_tool SYSTEM PRIVATE "${CPM_PACKAGE_Taskflow_SOURCE_DIR}")
  endif()
endif()

target_link_libraries(octaryn_engine_runtime
  PRIVATE
    octaryn_engine_log
    octaryn_engine_diagnostics
    octaryn_engine_imgui_backend
    octaryn_engine_memory
    octaryn::deps::sdl3
    octaryn::deps::sdl3_image
    octaryn::deps::openal
    octaryn::deps::miniaudio
    octaryn::deps::imgui
    octaryn::deps::eigen
    octaryn::deps::glaze
    octaryn::deps::fastnoise2
    octaryn::deps::jolt
    octaryn::deps::ozz_animation
    octaryn::deps::unordered_dense
    octaryn::deps::lz4
    octaryn::deps::zlib
    octaryn::deps::zstd
    octaryn::deps::taskflow
    ${CMAKE_DL_LIBS})

if(OCTARYN_ENABLE_MANAGED_DOTNET)
  target_link_libraries(octaryn_engine_runtime PRIVATE "${OCTARYN_DOTNET_NETHOST_LIBRARY}")
endif()

if(OCTARYN_ENABLE_TRACY OR OCTARYN_ENABLE_PROFILING)
  target_link_libraries(octaryn_engine_runtime PRIVATE octaryn::deps::tracy)
  target_compile_definitions(octaryn_engine_runtime PRIVATE OCTARYN_ENABLE_TRACY=1)
endif()

target_compile_features(octaryn_engine_runtime PRIVATE cxx_std_23)
set_target_properties(octaryn_engine_runtime PROPERTIES OUTPUT_NAME octaryn-engine-runtime)
octaryn_enable_default_warnings(octaryn_engine_runtime)
octaryn_apply_build_layout(octaryn_engine_runtime PRODUCT octaryn-engine)
octaryn_product_preset_root(octaryn_engine_runtime_preset_root "octaryn-engine")
set(octaryn_engine_runtime_asset_dir "${octaryn_engine_runtime_preset_root}/bin/assets")
if(OCTARYN_ENABLE_MANAGED_DOTNET)
  set(octaryn_managed_client_host_project "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj")
  set(octaryn_managed_publish_dir "${octaryn_engine_runtime_preset_root}/bin/dotnet/client")
  set(octaryn_managed_publish_output "${octaryn_managed_publish_dir}/Octaryn.Client.dll")
  set(octaryn_managed_build_configuration Debug)
  if(CMAKE_BUILD_TYPE MATCHES "^[Rr]el")
    set(octaryn_managed_build_configuration Release)
  endif()
  file(GLOB_RECURSE octaryn_managed_project_files CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/*.cs"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/*.csproj"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/*.cs"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/*.csproj"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/*.cs"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/*.csproj"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Build.props"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Build.targets"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Packages.props"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/Octaryn.DotNet.sln")

  add_custom_command(
    OUTPUT "${octaryn_managed_publish_output}"
    COMMAND "${OCTARYN_DOTNET_EXECUTABLE}" publish "${octaryn_managed_client_host_project}"
      --configuration "${octaryn_managed_build_configuration}"
      --framework net10.0
      --output "${octaryn_managed_publish_dir}"
      --no-self-contained
    DEPENDS
      "${OCTARYN_WORKSPACE_ROOT_DIR}/global.json"
      ${octaryn_managed_project_files}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

  add_custom_target(octaryn_managed_client_host DEPENDS "${octaryn_managed_publish_output}")
  add_dependencies(octaryn_engine_runtime octaryn_managed_client_host)
endif()

if(OCTARYN_ENABLE_RUNTIME_ASSET_GENERATION)
  add_custom_command(TARGET octaryn_engine_runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_atlas}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_normal_atlas}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas_n.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_specular_atlas}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas_s.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_atlas_manifest}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_animation_atlas}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas_anim.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_animation_manifest}"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/textures/atlas_anim.txt"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
      "$<TARGET_FILE_DIR:octaryn_engine_runtime>/assets/shaders"
    VERBATIM)

  add_custom_target(octaryn_engine_runtime_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${octaryn_engine_runtime_asset_dir}/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${octaryn_engine_runtime_asset_dir}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_atlas}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_normal_atlas}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas_n.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_specular_atlas}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas_s.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_atlas_manifest}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_animation_atlas}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas_anim.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${octaryn_engine_generated_animation_manifest}"
      "${octaryn_engine_runtime_asset_dir}/textures/atlas_anim.txt"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
      "${octaryn_engine_runtime_asset_dir}/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${octaryn_engine_runtime_asset_dir}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
      "${octaryn_engine_runtime_asset_dir}/shaders"
    DEPENDS octaryn_engine_shader_assets
            octaryn_engine_texture_atlas
    VERBATIM)

  add_dependencies(octaryn_engine_runtime octaryn_engine_texture_atlas octaryn_engine_runtime_assets)
  add_custom_target(octaryn_engine_runtime_bundle DEPENDS octaryn_engine_runtime octaryn_engine_runtime_assets)
else()
  add_custom_target(octaryn_engine_runtime_bundle DEPENDS octaryn_engine_runtime)
endif()

install(TARGETS octaryn_engine_runtime
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(OCTARYN_ENABLE_RUNTIME_ASSET_GENERATION)
  install(FILES ${octaryn_engine_generated_texture_outputs}
          DESTINATION "${CMAKE_INSTALL_BINDIR}/assets/textures")

  install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/"
          DESTINATION "${CMAKE_INSTALL_BINDIR}/assets/shaders")

  install(TARGETS octaryn_engine_shader_tool
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
