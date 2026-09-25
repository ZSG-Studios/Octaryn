include_guard(GLOBAL)

function(octaryn_add_dotnet_owner target_name owner project_path)
    octaryn_owner_build_root(owner_build_root "${owner}")
    octaryn_owner_managed_root(owner_managed_root "${owner}")
    octaryn_owner_log_root(owner_log_root "${owner}")
    get_filename_component(project_file_name "${project_path}" NAME)
    get_filename_component(project_dir "${project_path}" DIRECTORY)
    string(REGEX REPLACE "\\.csproj$" "" project_stem "${project_file_name}")
    set(dotnet_configuration "${CMAKE_BUILD_TYPE}")
    if(NOT dotnet_configuration)
        set(dotnet_configuration "Debug")
    endif()
    set(stamp_dir "${owner_build_root}/stamps")
    set(stamp_file "${stamp_dir}/${target_name}.stamp")
    set(output_dll "${owner_managed_root}/${project_stem}.dll")
    file(GLOB_RECURSE dotnet_source_inputs CONFIGURE_DEPENDS
        "${project_dir}/*.cs"
        "${project_dir}/*.csproj")
    file(GLOB_RECURSE dotnet_validation_inputs CONFIGURE_DEPENDS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/*.py"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/package-policy/*")

    file(MAKE_DIRECTORY "${owner_log_root}" "${stamp_dir}")

    add_custom_command(
        OUTPUT "${stamp_file}"
        BYPRODUCTS "${output_dll}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
            "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
            "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
            "${DOTNET_EXECUTABLE}" build "${project_path}"
            --configuration "${dotnet_configuration}"
            --no-dependencies
            -maxcpucount
            ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
            "-bl:${owner_log_root}/${target_name}-${OCTARYN_BUILD_PRESET_NAME}.binlog"
        COMMAND "${CMAKE_COMMAND}" -E touch "${stamp_file}"
        DEPENDS
            ${dotnet_source_inputs}
            "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Build.props"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Build.targets"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/Directory.Packages.props"
            ${dotnet_validation_inputs}
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)

    add_custom_target(${target_name}
        DEPENDS "${stamp_file}")

    set(${target_name}_STAMP "${stamp_file}" PARENT_SCOPE)
    set(${target_name}_OUTPUT_DLL "${output_dll}" PARENT_SCOPE)
endfunction()
