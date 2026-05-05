if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    octaryn_add_native_shared_library(
        octaryn_client_managed_bridge
        client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/NativeLoading/ManagedBridge.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/LaunchProbe/LaunchProbe.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/AtlasFallbackDraw/AtlasFallbackDraw.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/BlockInteraction/BlockInteraction.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/CompositePass/CompositePass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Environment/Environment.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/EventPump/EventPump.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/RuntimeFiles/FileIO.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/FrameLoop/FrameLoop.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/FrameLogs/FrameLogs.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/FrameRender/FrameRender.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/FrameTargets/FrameTargets.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/HostCommands/HostCommands.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Input/Input.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Logging/Log.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/EmptyWorldAtlas/EmptyWorldAtlas.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/PresentationSnapshots/PresentationSnapshots.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/PresentationState/PresentationState.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/ShaderPipelines/ShaderPipelines.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/ShaderWorldPass/ShaderWorldPass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/SkyUniforms/SkyUniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/UiOverlay/UiOverlayPass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/UiOverlay/UiOverlayUniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Window/Window.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldIntents/WorldIntents.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldMeshRuntime/WorldMeshRuntime.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldStream/WorldStream.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/SingleplayerServerSession/SingleplayerServerSession.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/HostApp.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/Blocks.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/Builder.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/TerrainMeshBatch.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/TerrainMesh.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Packing/Packing.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/View/View.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/WorldMeshUpload/WorldMeshUpload.cpp"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/AtlasFallbackDraw"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/BlockInteraction"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/CompositePass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Environment"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/EventPump"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/RuntimeFiles"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/FrameLoop"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/FrameLogs"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/FrameRender"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/FrameTargets"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/HostCommands"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Input"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Logging"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/EmptyWorldAtlas"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/PresentationSnapshots"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/PresentationState"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/ShaderPipelines"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/ShaderWorldPass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Rendering/SkyUniforms"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/UiOverlay"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Window"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldIntents"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldMeshRuntime"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldStream"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/SingleplayerServerSession"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/AssetPaths"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameProfile"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FunctionProfile"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Diagnostics/FrameMetrics"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Input/PlayerControl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Player/FlyController"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/BlockAtlas/Atlas"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/BlockAtlas/Files"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/BlockAtlas/Textures"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Camera"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Shaders/Create"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Shaders/Metadata"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Packing"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/Planning"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/EmptyWorldMesh/View"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/WorldMeshUpload"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RuntimeSettings"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Entrypoints"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Events"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RuntimeControls/Menu"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/FramePacing"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Swapchain"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/ChunkView"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Window/Lifecycle"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_client_asset_paths
            octaryn_client_block_atlas
            octaryn_client_camera
            octaryn_client_chunk_mesh_plan
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
            octaryn_native_jobs
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
