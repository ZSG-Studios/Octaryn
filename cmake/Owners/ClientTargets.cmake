include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)
include(Dependencies/ClientDependencies)

octaryn_owner_build_root(client_build_root client)
octaryn_owner_build_root(client_app_probe_server_build_root server)
octaryn_owner_log_root(client_log_root client)
set(octaryn_client_bundle_dir "${client_build_root}/bundle")
set(octaryn_client_bundle_obj_dir "${client_build_root}/bundle-obj")
set(octaryn_client_bundle_stamp "${client_build_root}/stamps/octaryn_client_bundle.stamp")
set(octaryn_client_app_bundle_stamp "${client_build_root}/stamps/octaryn_client_app_bundle.stamp")
set(octaryn_client_server_dir "${octaryn_client_bundle_dir}/server")
set(octaryn_client_server_app_stamp "${client_build_root}/stamps/octaryn_client_server_app.stamp")
set(octaryn_client_bundle_output "${octaryn_client_bundle_dir}/Octaryn.Client.dll")
set(octaryn_client_app_bundle_output "${octaryn_client_bundle_dir}/Octaryn.Client${CMAKE_EXECUTABLE_SUFFIX}")
set(octaryn_client_managed_bridge_bundle_output "${octaryn_client_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_client_managed_bridge${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(octaryn_client_app_probe_log "${client_log_root}/octaryn_client_app_launch_probe-${OCTARYN_BUILD_PRESET_NAME}.log")
set(octaryn_client_app_probe_world_blocks "${client_app_probe_server_build_root}/validation/client-server-app-launch-probe-world/world_blocks.json")
set(octaryn_client_shader_stage_dir "${client_build_root}/shaders/source")
set(octaryn_client_shader_stage_stamp "${client_build_root}/stamps/octaryn_client_shaders.stamp")

octaryn_add_native_owner(octaryn_client_native)
add_dependencies(octaryn_client_native octaryn_shared_native)

file(GLOB_RECURSE octaryn_client_shader_sources CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/*")
list(FILTER octaryn_client_shader_sources EXCLUDE REGEX "/\\.gitkeep$")
set(octaryn_client_shader_bundle_outputs)
foreach(octaryn_client_shader_source IN LISTS octaryn_client_shader_sources)
    file(RELATIVE_PATH octaryn_client_shader_file
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_source}")
    list(APPEND octaryn_client_shader_bundle_outputs
        "${octaryn_client_bundle_dir}/Client/Shaders/${octaryn_client_shader_file}")
endforeach()

add_custom_command(
    OUTPUT "${octaryn_client_shader_stage_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_shader_stage_stamp}"
    DEPENDS ${octaryn_client_shader_sources}
    VERBATIM)

add_custom_target(octaryn_client_shaders
    DEPENDS "${octaryn_client_shader_stage_stamp}")

octaryn_add_native_static_library(
    octaryn_client_asset_paths
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/AssetPaths/octaryn_client_asset_path.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/AssetPaths"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_asset_paths
        PRIVATE
            OCTARYN_CLIENT_ASSET_PATHS_USE_SDL3)
endif()

add_dependencies(octaryn_client_native octaryn_client_asset_paths)

octaryn_add_native_static_library(
    octaryn_client_host_environment
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHost/Environment/octaryn_client_host_environment.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHost/Environment"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_host_environment)

octaryn_add_native_static_library(
    octaryn_client_render_distance
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/RenderDistance/octaryn_client_render_distance.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/RenderDistance")

add_dependencies(octaryn_client_native octaryn_client_render_distance)

octaryn_add_native_static_library(
    octaryn_client_frame_metrics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/FrameMetrics/octaryn_client_frame_metrics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/FrameMetrics")

add_dependencies(octaryn_client_native octaryn_client_frame_metrics)

octaryn_add_native_static_library(
    octaryn_client_app_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/AppSettings/octaryn_client_app_settings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/AppSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/RenderDistance"
    PRIVATE_LINKS
        octaryn_client_render_distance)

add_dependencies(octaryn_client_native octaryn_client_app_settings)

octaryn_add_native_static_library(
    octaryn_client_lighting_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/LightingSettings/octaryn_client_lighting_settings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/LightingSettings")

add_dependencies(octaryn_client_native octaryn_client_lighting_settings)

octaryn_add_native_static_library(
    octaryn_client_display_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/DisplaySettings/octaryn_client_display_settings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/DisplaySettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/AppSettings"
    PRIVATE_LINKS
        octaryn_client_app_settings
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_display_settings)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_display_settings
        PUBLIC
            OCTARYN_CLIENT_DISPLAY_SETTINGS_USE_SDL3)
endif()

octaryn_add_native_static_library(
    octaryn_client_display_catalog
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/DisplayCatalog/octaryn_client_display_catalog.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/DisplayCatalog"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_display_catalog)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_display_catalog
        PUBLIC
            OCTARYN_CLIENT_DISPLAY_CATALOG_USE_SDL3)
endif()

octaryn_add_native_static_library(
    octaryn_client_fullscreen_display_mode
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FullscreenDisplayMode/octaryn_client_fullscreen_display_mode.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FullscreenDisplayMode"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_fullscreen_display_mode)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_fullscreen_display_mode
        PUBLIC
            OCTARYN_CLIENT_FULLSCREEN_DISPLAY_MODE_USE_SDL3)
endif()

octaryn_add_native_static_library(
    octaryn_client_window_lifecycle
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Lifecycle/octaryn_client_window_lifecycle.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Lifecycle"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FullscreenDisplayMode"
    PRIVATE_LINKS
        octaryn_client_fullscreen_display_mode
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_window_lifecycle)

octaryn_add_native_static_library(
    octaryn_client_frame_pacing
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FramePacing/octaryn_client_frame_pacing.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FramePacing"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_frame_pacing)

octaryn_add_native_static_library(
    octaryn_client_swapchain
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Swapchain/octaryn_client_swapchain.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Swapchain"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FramePacing"
    PRIVATE_LINKS
        octaryn_client_frame_pacing
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_swapchain)

octaryn_add_native_static_library(
    octaryn_client_window_frame_statistics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FrameStatistics/octaryn_client_window_frame_statistics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FrameStatistics")

add_dependencies(octaryn_client_native octaryn_client_window_frame_statistics)

octaryn_add_native_static_library(
    octaryn_client_display_menu
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Ui/DisplayMenu/octaryn_client_display_menu.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Ui/DisplayMenu")

add_dependencies(octaryn_client_native octaryn_client_display_menu)

octaryn_add_native_static_library(
    octaryn_client_camera_matrix
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera/octaryn_client_camera_matrix.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera")

add_dependencies(octaryn_client_native octaryn_client_camera_matrix)

octaryn_add_native_static_library(
    octaryn_client_camera
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera/octaryn_client_camera.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera"
    PRIVATE_LINKS
        octaryn_client_camera_matrix)

add_dependencies(octaryn_client_native octaryn_client_camera)

octaryn_add_native_static_library(
    octaryn_client_visibility_flags
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Visibility/octaryn_client_visibility_flags.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Visibility")

add_dependencies(octaryn_client_native octaryn_client_visibility_flags)

octaryn_add_native_static_library(
    octaryn_client_hidden_block_uniforms
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Scene/HiddenBlocks/octaryn_client_hidden_block_uniforms.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Scene/HiddenBlocks")

add_dependencies(octaryn_client_native octaryn_client_hidden_block_uniforms)

octaryn_add_native_static_library(
    octaryn_client_shader_metadata_contract
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Metadata/octaryn_client_shader_metadata_contract.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Metadata")

add_dependencies(octaryn_client_native octaryn_client_shader_metadata_contract)

octaryn_add_native_static_library(
    octaryn_client_shader_creation
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Create/octaryn_client_shader_creation.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Create"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Metadata"
    PRIVATE_LINKS
        octaryn_client_shader_metadata_contract
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_shader_creation)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_shader_creation
        PUBLIC
            OCTARYN_CLIENT_SHADER_CREATION_USE_SDL3)
endif()

if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    octaryn_add_native_shared_library(
        octaryn_client_managed_bridge
        client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ManagedBridge/octaryn_client_managed_bridge.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_shared_host_abi
            octaryn_native_diagnostics
            octaryn::dotnet_hosting)

    target_compile_definitions(octaryn_client_managed_bridge
        PRIVATE
            OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH="${octaryn_client_bundle_dir}/Octaryn.Client.dll"
            OCTARYN_CLIENT_RUNTIME_CONFIG_PATH="${octaryn_client_bundle_dir}/Octaryn.Client.runtimeconfig.json")

    add_dependencies(octaryn_client_native octaryn_client_managed_bridge)

    octaryn_add_native_executable(
        octaryn_client_launch_probe
        client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/LaunchProbe/octaryn_client_launch_probe.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_client_managed_bridge
            octaryn_native_diagnostics)

    target_compile_definitions(octaryn_client_launch_probe
        PRIVATE
            OCTARYN_CLIENT_LAUNCH_PROBE_LOG_PATH="${client_log_root}/octaryn_client_launch_probe-${OCTARYN_BUILD_PRESET_NAME}.log")

    add_dependencies(octaryn_client_native octaryn_client_launch_probe)

    octaryn_add_native_executable(
        octaryn_client_app
        client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/octaryn_client_app.cpp"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Lifecycle"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_client_managed_bridge
            octaryn_client_window_lifecycle
            octaryn_native_diagnostics
            octaryn::deps::glaze
            octaryn::deps::sdl3)

    set_target_properties(octaryn_client_app PROPERTIES
        OUTPUT_NAME "Octaryn.Client"
        BUILD_RPATH "$ORIGIN")

    add_dependencies(octaryn_client_native octaryn_client_app)
else()
    add_custom_target(octaryn_client_managed_bridge
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client managed bridge: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
    add_custom_target(octaryn_client_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client launch probe binary: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
    add_custom_target(octaryn_client_app
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client graphical app: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()

octaryn_add_dotnet_owner(
    octaryn_client_managed
    client
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj")

add_dependencies(octaryn_client_managed octaryn_shared octaryn_basegame)

add_custom_command(
    OUTPUT "${octaryn_client_managed_STAMP}"
    APPEND
    DEPENDS
        "${octaryn_shared_STAMP}"
        "${octaryn_basegame_STAMP}")

file(MAKE_DIRECTORY "${client_build_root}/stamps" "${client_log_root}")

set(octaryn_client_app_bundle_outputs)
set(octaryn_client_app_bundle_commands)
set(octaryn_client_app_bundle_depends)
if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    list(APPEND octaryn_client_app_bundle_outputs
        "${octaryn_client_app_bundle_output}"
        "${octaryn_client_managed_bridge_bundle_output}")
    list(APPEND octaryn_client_app_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:octaryn_client_app>"
            "${octaryn_client_app_bundle_output}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:octaryn_client_managed_bridge>"
            "${octaryn_client_managed_bridge_bundle_output}")
    list(APPEND octaryn_client_app_bundle_depends octaryn_client_app)
endif()

add_custom_command(
    OUTPUT "${octaryn_client_app_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_client_bundle_output}"
        ${octaryn_client_app_bundle_outputs}
        "${octaryn_client_bundle_dir}/Octaryn.Client.deps.json"
        "${octaryn_client_bundle_dir}/Octaryn.Client.runtimeconfig.json"
        "${octaryn_client_bundle_dir}/Octaryn.Basegame.dll"
        "${octaryn_client_bundle_dir}/Octaryn.Basegame.deps.json"
        "${octaryn_client_bundle_dir}/Octaryn.Basegame.runtimeconfig.json"
        "${octaryn_client_bundle_dir}/Data/Module/octaryn.basegame.module.json"
        "${octaryn_client_bundle_dir}/Data/Blocks/octaryn.basegame.blocks.json"
        "${octaryn_client_bundle_dir}/Data/Biomes/octaryn.basegame.biomes.json"
        "${octaryn_client_bundle_dir}/Data/Features/octaryn.basegame.features.json"
        "${octaryn_client_bundle_dir}/Data/Items/octaryn.basegame.item.hand.json"
        "${octaryn_client_bundle_dir}/Data/Rules/octaryn.basegame.rule.default_interaction.json"
        "${octaryn_client_bundle_dir}/Data/Rules/octaryn.basegame.rule.terrain_generation.json"
        ${octaryn_client_shader_bundle_outputs}
        "${octaryn_client_bundle_dir}/Octaryn.Shared.dll"
        "${octaryn_client_bundle_dir}/Octaryn.Client.pdb"
        "${octaryn_client_bundle_dir}/Octaryn.Basegame.pdb"
        "${octaryn_client_bundle_dir}/Octaryn.Shared.pdb"
        "${octaryn_client_bundle_dir}/Arch.dll"
        "${octaryn_client_bundle_dir}/Arch.EventBus.dll"
        "${octaryn_client_bundle_dir}/Arch.LowLevel.dll"
        "${octaryn_client_bundle_dir}/Arch.Relationships.dll"
        "${octaryn_client_bundle_dir}/Arch.System.dll"
        "${octaryn_client_bundle_dir}/Collections.Pooled.dll"
        "${octaryn_client_bundle_dir}/CommunityToolkit.HighPerformance.dll"
        "${octaryn_client_bundle_dir}/Microsoft.Extensions.ObjectPool.dll"
        "${octaryn_client_bundle_dir}/Schedulers.dll"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_client_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" restore "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj"
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_client_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" publish "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --framework net10.0
        --output "${octaryn_client_bundle_dir}"
        --no-self-contained
        --no-restore
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${client_log_root}/octaryn_client_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    ${octaryn_client_app_bundle_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${octaryn_basegame_bundle_dir}"
        "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_bundle_dir}/Client/Shaders"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_app_bundle_stamp}"
    DEPENDS
        "${octaryn_client_shader_stage_stamp}"
        ${octaryn_client_shader_sources}
        octaryn_basegame_bundle
        ${octaryn_client_app_bundle_depends}
        "${octaryn_client_managed_STAMP}"
        "${octaryn_shared_STAMP}"
        "${octaryn_basegame_STAMP}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_command(
    OUTPUT "${octaryn_client_server_app_stamp}"
    BYPRODUCTS
        "${octaryn_client_server_dir}/Octaryn.Server.dll"
        "${octaryn_client_server_dir}/Octaryn.Server.deps.json"
        "${octaryn_client_server_dir}/Octaryn.Server.runtimeconfig.json"
        "${octaryn_client_server_dir}/Octaryn.Server${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_client_server_dir}/Octaryn.Shared.dll"
        "${octaryn_client_server_dir}/Octaryn.Basegame.dll"
        "${octaryn_client_server_dir}/Data/Module/octaryn.basegame.module.json"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${octaryn_bundled_server_app_source_dir}"
        "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_server_app_stamp}"
    DEPENDS
        "${octaryn_client_app_bundle_stamp}"
        "${octaryn_bundled_server_app_source_stamp}"
        ${octaryn_bundled_server_app_source_target}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_client_server_app
    DEPENDS "${octaryn_client_server_app_stamp}")

add_custom_command(
    OUTPUT "${octaryn_client_bundle_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_bundle_stamp}"
    DEPENDS
        "${octaryn_client_server_app_stamp}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_client_bundle
    DEPENDS "${octaryn_client_bundle_stamp}")

add_dependencies(octaryn_client_bundle
    octaryn_client_server_app
    octaryn_client_native)

if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    add_custom_target(octaryn_run_client_launch_probe
        COMMAND "$<TARGET_FILE:octaryn_client_launch_probe>"
        DEPENDS
            octaryn_client_bundle
            octaryn_client_launch_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_run_client_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client launch probe: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()

if(OCTARYN_DOTNET_HOSTING_AVAILABLE AND OCTARYN_TARGET_PLATFORM STREQUAL "Linux" AND OCTARYN_TARGET_ARCH STREQUAL "x64")
    add_custom_target(octaryn_run_client_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E env
            "SDL_VIDEODRIVER=dummy"
            "OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES=6"
            "OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH=${octaryn_client_app_probe_world_blocks}"
            "OCTARYN_CLIENT_APP_VALIDATE_PIXELS=1"
            "OCTARYN_CLIENT_APP_LOG_PATH=${octaryn_client_app_probe_log}"
            "${octaryn_client_app_bundle_output}"
        DEPENDS
            octaryn_client_bundle
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_client_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E env
            "SDL_VIDEODRIVER=dummy"
            "OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES=6"
            "OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH=${octaryn_client_app_probe_world_blocks}"
            "OCTARYN_CLIENT_APP_VALIDATE_PIXELS=1"
            "OCTARYN_CLIENT_APP_LOG_PATH=${octaryn_client_app_probe_log}"
            "${octaryn_client_app_bundle_output}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_app_launch_probe_log.py"
            --log-file "${octaryn_client_app_probe_log}"
        DEPENDS
            octaryn_client_bundle
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_run_client_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client app launch probe: graphical client host execution is only active for Linux/x64 targets with .NET native hosting."
        VERBATIM)
    add_custom_target(octaryn_validate_client_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client app launch probe validation: graphical client host execution is only active for Linux/x64 targets with .NET native hosting."
        VERBATIM)
endif()
