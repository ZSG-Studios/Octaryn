if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    add_custom_target(octaryn_run_client_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E env
            "$<TARGET_FILE:octaryn_client_launch_probe>"
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
            "SDL_VIDEODRIVER=wayland"
            "OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES=6"
            "OCTARYN_CLIENT_APP_INPUT_PROBE=1"
            "OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH=${octaryn_client_app_probe_world_blocks}"
            "OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH=${octaryn_client_app_probe_chunk_stream}"
            "OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH=${octaryn_client_app_probe_chunk_view_intent}"
            "OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH=${octaryn_client_app_probe_player_input_intent}"
            "OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH=${octaryn_client_app_probe_block_interaction_intent}"
            "OCTARYN_CLIENT_APP_VALIDATE_PIXELS=1"
            "OCTARYN_CLIENT_APP_LOG_PATH=${octaryn_client_app_probe_log}"
            "${octaryn_client_app_bundle_output}"
        DEPENDS
            octaryn_client_bundle
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_client_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E env
            "SDL_VIDEODRIVER=wayland"
            "OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES=6"
            "OCTARYN_CLIENT_APP_INPUT_PROBE=1"
            "OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH=${octaryn_client_app_probe_world_blocks}"
            "OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH=${octaryn_client_app_probe_chunk_stream}"
            "OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH=${octaryn_client_app_probe_chunk_view_intent}"
            "OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH=${octaryn_client_app_probe_player_input_intent}"
            "OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH=${octaryn_client_app_probe_block_interaction_intent}"
            "OCTARYN_CLIENT_APP_VALIDATE_PIXELS=1"
            "OCTARYN_CLIENT_APP_LOG_PATH=${octaryn_client_app_probe_log}"
            "${octaryn_client_app_bundle_output}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_app_launch_probe_log.py"
            --log-file "${octaryn_client_app_probe_log}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_server_app_readiness.py"
            --client-bundle-root "${octaryn_client_bundle_dir}"
            --world-blocks-path "${octaryn_client_app_probe_client_intent_world_blocks}"
            --chunk-view-intent-path "${octaryn_client_app_probe_chunk_view_intent}"
            --chunk-stream-path "${octaryn_client_app_probe_client_intent_chunk_stream}"
            --player-input-intent-path "${octaryn_client_app_probe_player_input_intent}"
            --block-interaction-intent-path "${octaryn_client_app_probe_block_interaction_intent}"
            --preserve-chunk-view-intent
            --log-file "${octaryn_client_app_probe_client_intent_server_log}"
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
