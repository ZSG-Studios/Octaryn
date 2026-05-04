add_custom_target(octaryn_validate_bundle_module_payload
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_bundle_module_payload.py"
        --bundle-root "${octaryn_tool_client_bundle_dir}"
        --module-id "octaryn.basegame"
        --expected-manifest "${octaryn_tool_basegame_manifest_json}"
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_bundle_module_payload.py"
        --bundle-root "${octaryn_tool_server_bundle_dir}"
        --module-id "octaryn.basegame"
        --expected-manifest "${octaryn_tool_basegame_manifest_json}"
    DEPENDS
        "${octaryn_tool_client_bundle_output}"
        "${octaryn_tool_server_bundle_output}"
        octaryn_client_bundle
        octaryn_server_bundle
        octaryn_validate_module_manifest_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_client_server_app
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_server_app.py"
        --client-bundle-root "${octaryn_tool_client_bundle_dir}"
        --server-bundle-root "${octaryn_tool_server_bundle_dir}"
    DEPENDS
        octaryn_client_server_app
        octaryn_server_bundle
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux" AND OCTARYN_TARGET_ARCH STREQUAL "x64")
    add_custom_target(octaryn_client_server_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_tool_client_server_app_probe_world_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_tool_client_server_app_probe_world_dir}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_server_app_readiness.py"
            --client-bundle-root "${octaryn_tool_client_bundle_dir}"
            --world-blocks-path "${octaryn_tool_client_server_app_probe_world_blocks}"
            --chunk-view-intent-path "${octaryn_tool_client_server_app_probe_chunk_view_intent}"
            --chunk-stream-path "${octaryn_tool_client_server_app_probe_chunk_stream}"
            --player-input-intent-path "${octaryn_tool_client_server_app_probe_player_input_intent}"
            --block-interaction-intent-path "${octaryn_tool_client_server_app_probe_block_interaction_intent}"
            --log-file "${octaryn_tool_client_server_app_probe_log}"
        DEPENDS
            octaryn_client_server_app
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_dependencies(octaryn_run_client_app_launch_probe octaryn_client_server_app_launch_probe)
    add_dependencies(octaryn_validate_client_app_launch_probe octaryn_client_server_app_launch_probe)
else()
    add_custom_target(octaryn_client_server_app_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping validate_client_server_app_readiness.py: client_server_app launch probe host execution is only active for Linux/x64 targets."
        VERBATIM)
endif()

add_custom_target(octaryn_validate_client_shader_bundle
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_shader_bundle.py"
        --source-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        --bundle-shader-root "${octaryn_tool_client_bundle_dir}/Client/Shaders"
    DEPENDS
        octaryn_client_bundle
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_native_abi_contracts
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_native_abi_contracts.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_native_owner_boundaries
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_native_owner_boundaries.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
