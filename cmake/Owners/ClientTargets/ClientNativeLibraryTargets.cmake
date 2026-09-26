include(Dependencies/GltfDependencies)
octaryn_add_native_owner(octaryn_client_native)
include(Owners/ClientTargets/ClientThreadingTargets)

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
    octaryn_client_action_audio
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio/ActionAudio.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio/Synthesis.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio/Miniaudio.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio"
    PRIVATE_LINKS octaryn::deps::openal octaryn::deps::miniaudio)
add_dependencies(octaryn_client_native octaryn_client_action_audio)

include(Owners/ClientTargets/ClientRenderBackendSources)
octaryn_add_native_static_library(
    octaryn_client_render_backend
    client
    SOURCES
        ${OCTARYN_CLIENT_RENDER_BACKEND_SOURCES}
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Ui"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Sky"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Hdr"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Temporal"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld"
    PRIVATE_LINKS
        octaryn_client_threading
        octaryn_native_jobs
        octaryn_client_asset_paths
        octaryn_client_runtime_controls
        octaryn_client_frame_profile
        octaryn_client_camera
        octaryn_client_lighting_settings
        octaryn::deps::glaze
        octaryn::deps::slang_rhi
        octaryn::deps::rmlui
        octaryn::deps::fastgltf
        octaryn::deps::meshoptimizer
        octaryn::deps::stb_image
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_render_backend)
include(Dependencies/Fsr2)
octaryn_configure_fsr2(octaryn_client_render_backend)

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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/AppSettings")

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
    octaryn_client_window_frame_statistics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FrameStatistics/FrameStatistics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FrameStatistics")

add_dependencies(octaryn_client_native octaryn_client_window_frame_statistics)

octaryn_add_native_static_library(
    octaryn_client_render_distance
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance/RenderDistance.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance")
add_dependencies(octaryn_client_native octaryn_client_render_distance)

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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Entrypoints/RuntimeControls.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Events/Events.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Menu/Menu.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Entrypoints"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Events"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Menu"
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
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Entrypoints"
    PRIVATE_LINKS
        octaryn_client_app_settings
        octaryn_client_runtime_controls
        octaryn_client_display_settings
        octaryn::deps::glaze
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_runtime_settings)

octaryn_add_native_static_library(
    octaryn_client_camera_matrix
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Camera/CameraMatrix.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Camera")

add_dependencies(octaryn_client_native octaryn_client_camera_matrix)

octaryn_add_native_static_library(
    octaryn_client_camera
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Camera/Camera.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Camera"
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
    octaryn_client_visibility_flags
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Visibility/VisibilityFlags.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Visibility"
    PRIVATE_LINKS
        octaryn::deps::sdl3)

add_dependencies(octaryn_client_native octaryn_client_visibility_flags)

if(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
    target_compile_definitions(octaryn_client_render_backend
        PRIVATE
            OCTARYN_CLIENT_SLANG_RHI_AVAILABLE=1)
endif()

if(OCTARYN_CLIENT_SDL3_AVAILABLE)
    target_compile_definitions(octaryn_client_render_backend
        PRIVATE
            OCTARYN_CLIENT_SDL3_AVAILABLE=1)
endif()

octaryn_add_native_static_library(
    octaryn_client_physics
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Physics/PhysicsWorld.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Physics"
    PRIVATE_LINKS
        octaryn::deps::box3d)

add_dependencies(octaryn_client_native octaryn_client_physics)

add_executable(octaryn_physics_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/PhysicsProbe/main.cpp")
target_link_libraries(octaryn_physics_probe PRIVATE octaryn_client_physics)
target_compile_features(octaryn_physics_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_physics_probe tools)

add_executable(octaryn_character_motion_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/PhysicsProbe/CharacterMotionProbe.cpp")
target_link_libraries(octaryn_character_motion_probe PRIVATE octaryn_character_motion)
target_compile_features(octaryn_character_motion_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_character_motion_probe tools)

octaryn_add_native_static_library(
    octaryn_client_world_streaming
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldStreaming/TileSet.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldStreaming"
    PRIVATE_LINKS
        octaryn::deps::glaze)

add_dependencies(octaryn_client_native octaryn_client_world_streaming)

