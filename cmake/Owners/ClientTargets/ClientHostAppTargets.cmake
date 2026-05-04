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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/AtlasFallbackDraw/AtlasFallbackDraw.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/BlockInteraction/BlockInteraction.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/CompositePass/CompositePass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Environment/Environment.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/EventPump/EventPump.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FileIO/FileIO.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLoop/FrameLoop.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLogs/FrameLogs.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameRender/FrameRender.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameTargets/FrameTargets.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/HostCommands/HostCommands.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Input/Input.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Log/Log.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/NativeEmptyAtlas/NativeEmptyAtlas.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/PresentationSnapshots/PresentationSnapshots.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/PresentationState/PresentationState.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ShaderPipelines/ShaderPipelines.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ShaderWorldPass/ShaderWorldPass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/SkyUniforms/SkyUniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/UiOverlayPass/UiOverlayPass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/UiOverlayUniforms/UiOverlayUniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Window/Window.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/WorldIntents/WorldIntents.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/WorldStream/WorldStream.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/SingleplayerServerSession/octaryn_singleplayer_server_session.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/octaryn_client_app.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_blocks.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_builder.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_packing.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_view.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/WorldMeshUpload/octaryn_client_world_mesh_upload.cpp"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/AtlasFallbackDraw"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/BlockInteraction"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/CompositePass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Environment"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/EventPump"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FileIO"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLoop"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLogs"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameRender"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameTargets"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/HostCommands"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Input"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/JsonFiles"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Log"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/NativeEmptyAtlas"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/PresentationSnapshots"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/PresentationState"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ShaderPipelines"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ShaderWorldPass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/SkyUniforms"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/UiOverlayPass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/UiOverlayUniforms"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/Window"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/WorldIntents"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/WorldStream"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/SingleplayerServerSession"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/AssetPaths"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/ClientHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Diagnostics/FrameProfile"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Diagnostics/FunctionProfile"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/FrameMetrics"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Input/PlayerControl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Player/FlyController"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Atlas"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Camera"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Create"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/Shaders/Metadata"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/WorldMeshUpload"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/RenderDistance"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Settings/RuntimeSettings"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Ui/RuntimeControls"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/FramePacing"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Swapchain"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/WorldStreaming"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Window/Lifecycle"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_client_asset_paths
            octaryn_client_block_atlas
            octaryn_client_camera
            octaryn_client_chunk_view
            octaryn_client_frame_metrics
            octaryn_client_frame_profile
            octaryn_client_function_profile
            octaryn_client_fly_player_controller
            octaryn_client_managed_bridge
            octaryn_client_runtime_controls
            octaryn_client_runtime_settings
            octaryn_client_shader_creation
            octaryn_client_swapchain
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
