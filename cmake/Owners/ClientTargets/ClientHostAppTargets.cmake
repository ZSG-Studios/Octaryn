octaryn_add_native_executable(
    octaryn_client_app
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/OpenWorld.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/AppPaths.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MapManifest.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MapPlayer.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/LightingState.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/LoadingScreen.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup/RendererStartup.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup/MapStartup.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/TemporalValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Controls.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/WorldProfile.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerView.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerPresentation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ActionSounds.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DebugOverlay/DebugOverlay.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DebugOverlay"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldStreaming"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Player"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/LightingSettings"
    PRIVATE_LINKS
        octaryn_client_render_backend
        octaryn_native_jobs
        octaryn_client_player_control_input
        octaryn_client_camera
        octaryn_client_frame_metrics
        octaryn_client_frame_profile
        octaryn_client_action_audio
        octaryn_client_runtime_controls
        octaryn_client_runtime_settings
        octaryn_client_lighting_settings
        octaryn_client_world_streaming
        octaryn::deps::rmlui_sdl
        octaryn::deps::glaze
        octaryn::deps::sdl3)

set_target_properties(octaryn_client_app PROPERTIES
    OUTPUT_NAME "Octaryn.Client"
    BUILD_RPATH "$ORIGIN")

add_dependencies(octaryn_client_native octaryn_client_app)
