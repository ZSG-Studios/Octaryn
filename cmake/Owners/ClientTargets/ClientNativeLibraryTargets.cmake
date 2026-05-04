octaryn_add_native_owner(octaryn_client_native)
add_dependencies(octaryn_client_native octaryn_shared_native)
octaryn_add_native_static_library(
    octaryn_client_asset_paths
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/AssetPaths/AssetPath.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/AssetPaths"
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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/ClientHost/Environment/HostEnvironment.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/ClientHost/Environment"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_host_environment)

octaryn_add_native_static_library(
    octaryn_client_render_distance
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance/RenderDistance.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance")

add_dependencies(octaryn_client_native octaryn_client_render_distance)

octaryn_add_native_static_library(
    octaryn_client_chunk_view
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/ChunkView/ChunkView.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/ChunkView"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance"
    PRIVATE_LINKS
        octaryn_client_render_distance)

add_dependencies(octaryn_client_native octaryn_client_chunk_view)

octaryn_add_native_static_library(
    octaryn_client_frame_metrics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameMetrics/FrameMetrics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameMetrics")

add_dependencies(octaryn_client_native octaryn_client_frame_metrics)

octaryn_add_native_static_library(
    octaryn_client_frame_profile
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameProfile/FrameProfile.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameProfile"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameMetrics"
    PRIVATE_LINKS
        octaryn_client_frame_metrics
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_frame_profile)

octaryn_add_native_static_library(
    octaryn_client_function_profile
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FunctionProfile/FunctionProfile.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FunctionProfile"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_function_profile)

octaryn_add_native_static_library(
    octaryn_client_app_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/AppSettings/AppSettings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/AppSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance"
    PRIVATE_LINKS
        octaryn_client_render_distance)

add_dependencies(octaryn_client_native octaryn_client_app_settings)

octaryn_add_native_static_library(
    octaryn_client_lighting_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/LightingSettings/LightingSettings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/LightingSettings")

add_dependencies(octaryn_client_native octaryn_client_lighting_settings)

octaryn_add_native_static_library(
    octaryn_client_display_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/DisplaySettings/DisplaySettings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/DisplaySettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/AppSettings"
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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog/DisplayCatalog.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_display_catalog)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_display_catalog
        PUBLIC
            DISPLAY_CATALOG_USE_SDL3)
endif()

octaryn_add_native_static_library(
    octaryn_client_fullscreen_display_mode
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FullscreenDisplayMode/FullscreenDisplayMode.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FullscreenDisplayMode"
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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Lifecycle/Lifecycle.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Lifecycle"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FullscreenDisplayMode"
    PRIVATE_LINKS
        octaryn_client_fullscreen_display_mode
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_window_lifecycle)

octaryn_add_native_static_library(
    octaryn_client_frame_pacing
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FramePacing/FramePacing.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FramePacing"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_frame_pacing)

octaryn_add_native_static_library(
    octaryn_client_swapchain
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Swapchain/Swapchain.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Swapchain"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FramePacing"
    PRIVATE_LINKS
        octaryn_client_frame_pacing
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_swapchain)

octaryn_add_native_static_library(
    octaryn_client_window_frame_statistics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FrameStatistics/FrameStatistics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FrameStatistics")

add_dependencies(octaryn_client_native octaryn_client_window_frame_statistics)

octaryn_add_native_static_library(
    octaryn_client_display_menu
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu/DisplayMenu.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu")

add_dependencies(octaryn_client_native octaryn_client_display_menu)

octaryn_add_native_static_library(
    octaryn_client_runtime_controls
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/RuntimeControls.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Events.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Menu.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Stub.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Lifecycle"
    PRIVATE_LINKS
        octaryn_client_display_catalog
        octaryn_client_display_menu
        octaryn_client_render_distance
        octaryn_client_window_lifecycle
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_runtime_controls)

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_runtime_controls
        PUBLIC
            RUNTIME_CONTROLS_USE_SDL3)
endif()

octaryn_add_native_static_library(
    octaryn_client_runtime_settings
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RuntimeSettings/RuntimeSettings.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RuntimeSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/AppSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog"
    PRIVATE_LINKS
        octaryn_client_app_settings
        octaryn_client_runtime_controls
        octaryn::deps::glaze
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_runtime_settings)

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
    octaryn_client_player_control_input
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Input/PlayerControl/PlayerControlInput.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Input/PlayerControl"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_player_control_input)

octaryn_add_native_static_library(
    octaryn_client_fly_player_controller
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Player/FlyController/FlyPlayerController.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Player/FlyController"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Input/PlayerControl"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera"
    PRIVATE_LINKS
        octaryn_client_camera
        octaryn_client_player_control_input)

add_dependencies(octaryn_client_native octaryn_client_fly_player_controller)

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
    octaryn_client_block_atlas
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/BlockAtlas/BlockAtlas.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/BlockAtlas/BundleFile.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/BlockAtlas/Catalog.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/BlockAtlas/Textures.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/BlockAtlas"
    PRIVATE_LINKS
        octaryn_client_asset_paths
        octaryn::deps::glaze
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_block_atlas)

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
