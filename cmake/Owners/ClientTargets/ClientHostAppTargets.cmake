octaryn_add_native_static_library(
    octaryn_client_local_session
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/LocalSession.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/Prediction.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/ServerProcess.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/SessionFiles.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession/SessionIo.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/Interaction"
    PRIVATE_LINKS octaryn::deps::glaze octaryn_character_motion octaryn_client_world_stream octaryn_client_block_interaction)

add_executable(octaryn_client_prediction_qualification EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/PredictionQualification/main.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/PredictionQualification/InputEvents.cpp")
target_include_directories(octaryn_client_prediction_qualification PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Input/PlayerControl")
target_link_libraries(octaryn_client_prediction_qualification PRIVATE octaryn_client_local_session octaryn_character_motion octaryn::deps::sdl3)
target_compile_features(octaryn_client_prediction_qualification PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_client_prediction_qualification tools)

octaryn_add_native_static_library(
    octaryn_client_world_stream
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream/WorldStream.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream/StreamSnapshot.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream/GenerateColumn.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream")
target_include_directories(octaryn_client_world_stream PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain")

octaryn_add_native_static_library(
    octaryn_client_block_interaction
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/Interaction/BlockInteraction.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/Interaction"
    PRIVATE_LINKS octaryn_client_world_stream octaryn::deps::glaze)

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

    octaryn_add_native_executable(
        octaryn_client_app
        client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Main.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/OpenWorld.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/MainMenu.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/WorldSession.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/LoadingScreen.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup/RendererStartup.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/WorldItemsValidation.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/BlockActionsValidation.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/TemporalValidation.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/Controls.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/WorldProfile.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerView.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerPresentation.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ActionSounds.cpp"
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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/GameUiInventoryValidation.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/Inventory.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/InventoryPersistence.cpp"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Startup"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/LightingPanel"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Player"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend"
        PRIVATE_LINKS
            octaryn_client_render_backend
            octaryn_native_jobs
            octaryn_client_player_control_input
            octaryn_client_camera
            octaryn_client_frame_metrics
            octaryn_client_frame_profile
            octaryn_client_local_session
            octaryn_client_world_stream
            octaryn_client_world_items
            octaryn_client_block_interaction
            octaryn_client_action_audio
            octaryn_client_runtime_controls
            octaryn_client_runtime_settings
            octaryn_client_lighting_settings
            octaryn::deps::rmlui_sdl
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
