if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    add_custom_target(octaryn_run_client_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E env
            "OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH=${octaryn_client_bundle_dir}/Octaryn.Client.dll"
            "OCTARYN_CLIENT_RUNTIME_CONFIG_PATH=${octaryn_client_bundle_dir}/Octaryn.Client.runtimeconfig.json"
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

# Exercise the same packaged standalone-RHI open world on Windows and Linux.
# Its bounded runner preserves an isolated world and rejects incomplete/error runs.
add_custom_target(octaryn_run_client_app_launch_probe)
add_dependencies(octaryn_run_client_app_launch_probe octaryn_validate_client_rhi_diagnostic)
add_custom_target(octaryn_validate_client_app_launch_probe)
add_dependencies(octaryn_validate_client_app_launch_probe octaryn_validate_client_rhi_diagnostic)
