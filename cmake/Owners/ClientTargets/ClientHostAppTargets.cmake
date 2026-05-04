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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppAtlasFallbackDraw/octaryn_client_app_atlas_fallback_draw.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppBlockInteraction/octaryn_client_app_block_interaction.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppCompositePass/octaryn_client_app_composite_pass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppEnvironment/octaryn_client_app_environment.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/EventPump/EventPump.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFileIO/octaryn_client_app_file_io.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLoop/FrameLoop.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFrameLogs/octaryn_client_app_frame_logs.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameRender/FrameRender.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFrameTargets/octaryn_client_app_frame_targets.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppHostCommands/octaryn_client_app_host_commands.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppInput/octaryn_client_app_input.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppLog/octaryn_client_app_log.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppNativeEmptyAtlas/octaryn_client_app_native_empty_atlas.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppPresentationSnapshots/octaryn_client_app_presentation_snapshots.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppPresentationState/octaryn_client_app_presentation_state.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppShaderPipelines/octaryn_client_app_shader_pipelines.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppShaderWorldPass/octaryn_client_app_shader_world_pass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppSkyUniforms/octaryn_client_app_sky_uniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppUiOverlayPass/octaryn_client_app_ui_overlay_pass.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppUiOverlayUniforms/octaryn_client_app_ui_overlay_uniforms.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWindow/octaryn_client_app_window.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWorldIntents/octaryn_client_app_world_intents.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWorldStream/octaryn_client_app_world_stream.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/SingleplayerServerSession/octaryn_singleplayer_server_session.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/octaryn_client_app.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_blocks.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_builder.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_packing.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/octaryn_client_native_empty_world_mesh_view.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/Rendering/WorldMeshUpload/octaryn_client_world_mesh_upload.cpp"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppAtlasFallbackDraw"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppBlockInteraction"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppCompositePass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppEnvironment"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/EventPump"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFileIO"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameLoop"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFrameLogs"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/FrameRender"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppFrameTargets"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppHostCommands"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppInput"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppJsonFiles"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppLog"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppNativeEmptyAtlas"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppPresentationSnapshots"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppPresentationState"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppShaderPipelines"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppShaderWorldPass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppSkyUniforms"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppUiOverlayPass"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppUiOverlayUniforms"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWindow"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWorldIntents"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Native/App/ClientAppWorldStream"
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
