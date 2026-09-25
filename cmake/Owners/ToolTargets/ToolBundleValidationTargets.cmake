add_custom_target(octaryn_validate_bundle_module_payload
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_bundle_module_payload.py"
        --bundle-root "${octaryn_tool_client_bundle_dir}"
        --module-id "octaryn.basegame"
        --expected-manifest "${octaryn_tool_basegame_manifest_json}"
    COMMAND "${Python3_EXECUTABLE}"
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
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_server_app.py"
        --client-bundle-root "${octaryn_tool_client_bundle_dir}"
        --server-bundle-root "${octaryn_tool_server_bundle_dir}"
    DEPENDS
        octaryn_client_server_app
        octaryn_server_bundle
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

# Launch the packaged dedicated server, connect the remote loopback probe
# through the client remote transport, and verify handshake, authoritative
# state, edit acknowledgement and reconnect.
add_custom_target(octaryn_validate_remote_loopback
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_remote_loopback.py"
        --server-bundle "${octaryn_tool_server_bundle_dir}"
        --client-bundle "${octaryn_tool_client_bundle_dir}"
        --work-root "${tool_server_build_root}/validation/remote-loopback"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
        --configuration "${CMAKE_BUILD_TYPE}"
    DEPENDS
        octaryn_client_bundle
        octaryn_server_bundle
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

# Execute the actual packaged RHI client in a fresh authoritative world.
add_custom_target(octaryn_validate_client_rhi_diagnostic
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_rhi_client_diagnostic.py"
        --client-bundle-root "${octaryn_tool_client_bundle_dir}"
        --evidence-root "${client_build_root}/validation/rhi-client"
    DEPENDS octaryn_client_bundle octaryn_client_server_app
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_client_rml_ui
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_rml_ui.py"
        --client-bundle-root "${octaryn_tool_client_bundle_dir}"
        --evidence-root "${client_log_root}/validation/rml-ui"
    DEPENDS octaryn_client_bundle octaryn_client_server_app
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

set(octaryn_validate_client_slang_shader_args)
if(OCTARYN_SLANGC_EXECUTABLE)
    list(APPEND octaryn_validate_client_slang_shader_args
        --slangc "${OCTARYN_SLANGC_EXECUTABLE}")
endif()

add_custom_target(octaryn_validate_client_slang_shaders
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_slang_shaders.py"
        --source-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        --output-dir "${OCTARYN_BUILD_PRESET_ROOT}/client/shader-validation"
        ${octaryn_validate_client_slang_shader_args}
    DEPENDS
        octaryn_client_shaders
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_client_shader_bundle
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_client_shader_bundle.py"
        --source-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        --bundle-shader-root "${octaryn_tool_client_bundle_dir}/Client/Shaders"
    DEPENDS
        octaryn_client_bundle
        octaryn_validate_client_slang_shaders
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_remote_wire
    COMMAND "${Python3_EXECUTABLE}" "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_remote_wire.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)

add_custom_target(octaryn_validate_native_abi_contracts
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_native_abi_contracts.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_native_owner_boundaries
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_native_owner_boundaries.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
