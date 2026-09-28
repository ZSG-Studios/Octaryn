
octaryn_add_native_static_library(
    octaryn_client_local_session
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/LocalSession.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/ServerProcess.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/SessionFiles.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/SessionIo.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/Prediction.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
    PRIVATE_LINKS
        octaryn::deps::glaze
        octaryn::deps::sdl3
        octaryn_character_motion
        octaryn::deps::box3d)

add_dependencies(octaryn_client_native octaryn_client_local_session)

if(OCTARYN_DOTNET_HOSTING_AVAILABLE AND TARGET octaryn_client_managed_bridge)
    target_include_directories(octaryn_client_local_session PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi")
    target_link_libraries(octaryn_client_local_session PRIVATE octaryn_client_managed_bridge)
    target_compile_definitions(octaryn_client_local_session PRIVATE OCTARYN_CLIENT_REMOTE_MANAGED=1)
endif()
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

    octaryn_stage_dotnet_host_runtime(octaryn_client_managed_bridge)

    add_dependencies(octaryn_client_native octaryn_client_managed_bridge)

    # Remote sessions drive the managed LiteNetLib transport through the
    # client bridge exports.
    target_include_directories(octaryn_client_local_session PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi")
    target_link_libraries(octaryn_client_local_session PRIVATE octaryn_client_managed_bridge)
    target_compile_definitions(octaryn_client_local_session PRIVATE OCTARYN_CLIENT_REMOTE_MANAGED=1)

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

    endif()

octaryn_add_native_executable(
    octaryn_client_app
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/OpenWorld.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MapManifest.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ModuleActionValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/LoadingScreen.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup/RendererStartup.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup/MapStartup.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/TemporalValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Controls.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/WorldProfile.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerView.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MainMenu.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ActionSounds.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Host/ModuleHost.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DebugOverlay/DebugOverlay.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RmlRuntime/RmlRuntime.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/LightingPanel/LightingPanel.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUi.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiEvents.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiUpdate.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiFsr.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiFsrValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiInventory.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiMenu.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiPointer.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiItemTarget.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiModuleActions.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiInventoryValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/Inventory.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/InventoryPersistence.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DebugOverlay"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/LightingPanel"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/LightingSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RuntimeSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/RmlRuntime"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldStreaming"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Host"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/LightingSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RuntimeSettings"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/DisplayMenu"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Display/DisplayCatalog"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion"
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
        octaryn_client_local_session
        octaryn_client_world_streaming
        octaryn::deps::rmlui_sdl
        octaryn::deps::glaze
        octaryn::deps::sdl3)

set_target_properties(octaryn_client_app PROPERTIES
    OUTPUT_NAME "Octaryn.Client"
    BUILD_RPATH "$ORIGIN")

if(OCTARYN_DOTNET_HOSTING_AVAILABLE AND TARGET octaryn_client_managed_bridge)
    # The production app drives managed game modules through the client bridge.
    target_include_directories(octaryn_client_app PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/HostBridge/Abi")
    target_link_libraries(octaryn_client_app PRIVATE octaryn_client_managed_bridge)
    target_compile_definitions(octaryn_client_app PRIVATE OCTARYN_CLIENT_REMOTE_MANAGED=1)
endif()

add_dependencies(octaryn_client_native octaryn_client_app)
include("${OCTARYN_WORKSPACE_ROOT_DIR}/cmake/Owners/GameplayRouteProbeTargets.cmake")
