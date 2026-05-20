octaryn_add_dotnet_owner(
    octaryn_client_managed
    client
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj")

add_dependencies(octaryn_client_managed octaryn_shared)

set(octaryn_client_game_module_stamp_depends)
set(octaryn_client_game_module_bundle_depends)
set(octaryn_client_game_module_bundle_commands)
if(DEFINED octaryn_default_game_module_target)
    add_dependencies(octaryn_client_managed "${octaryn_default_game_module_target}")
endif()
if(DEFINED octaryn_default_game_module_stamp)
    list(APPEND octaryn_client_game_module_stamp_depends
        "${octaryn_default_game_module_stamp}")
endif()
if(DEFINED octaryn_default_game_module_bundle_dir)
    list(APPEND octaryn_client_game_module_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${octaryn_default_game_module_bundle_dir}"
            "${octaryn_client_bundle_dir}")
endif()
if(DEFINED octaryn_default_game_module_bundle_target)
    list(APPEND octaryn_client_game_module_bundle_depends
        "${octaryn_default_game_module_bundle_target}")
endif()
if(DEFINED octaryn_default_game_module_bundle_stamp)
    list(APPEND octaryn_client_game_module_bundle_depends
        "${octaryn_default_game_module_bundle_stamp}")
endif()

add_custom_command(
    OUTPUT "${octaryn_client_managed_STAMP}"
    APPEND
    DEPENDS
        "${octaryn_shared_STAMP}"
        ${octaryn_client_game_module_stamp_depends})

file(MAKE_DIRECTORY "${client_build_root}/stamps" "${client_log_root}")

set(octaryn_client_app_bundle_outputs)
set(octaryn_client_app_bundle_commands)
set(octaryn_client_app_bundle_depends)
if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    list(APPEND octaryn_client_app_bundle_outputs
        "${octaryn_client_app_bundle_output}"
        "${octaryn_client_managed_bridge_bundle_output}")
    list(APPEND octaryn_client_app_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:octaryn_client_app>"
            "${octaryn_client_app_bundle_output}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:octaryn_client_managed_bridge>"
            "${octaryn_client_managed_bridge_bundle_output}")
    list(APPEND octaryn_client_app_bundle_depends
        octaryn_client_app)
endif()

add_custom_command(
    OUTPUT "${octaryn_client_app_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_client_bundle_output}"
        ${octaryn_client_app_bundle_outputs}
        "${octaryn_client_bundle_dir}/Octaryn.Client.deps.json"
        "${octaryn_client_bundle_dir}/Octaryn.Client.runtimeconfig.json"
        ${octaryn_client_shader_bundle_outputs}
        "${octaryn_client_bundle_dir}/Octaryn.Shared.dll"
        "${octaryn_client_bundle_dir}/Octaryn.Client.pdb"
        "${octaryn_client_bundle_dir}/Octaryn.Shared.pdb"
        "${octaryn_client_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_client_bundle_dir}/Arch.dll"
        "${octaryn_client_bundle_dir}/Arch.EventBus.dll"
        "${octaryn_client_bundle_dir}/Arch.LowLevel.dll"
        "${octaryn_client_bundle_dir}/Arch.Relationships.dll"
        "${octaryn_client_bundle_dir}/Arch.System.dll"
        "${octaryn_client_bundle_dir}/Collections.Pooled.dll"
        "${octaryn_client_bundle_dir}/CommunityToolkit.HighPerformance.dll"
        "${octaryn_client_bundle_dir}/Microsoft.Extensions.ObjectPool.dll"
        "${octaryn_client_bundle_dir}/Schedulers.dll"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_client_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" restore "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj"
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_client_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" publish "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Octaryn.Client.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --framework net10.0
        --output "${octaryn_client_bundle_dir}"
        --no-self-contained
        --no-restore
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${client_log_root}/octaryn_client_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    ${octaryn_client_app_bundle_commands}
    ${octaryn_client_game_module_bundle_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_native_jobs>"
        "${octaryn_client_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_bundle_dir}/Client/Shaders"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets"
        "${octaryn_client_bundle_dir}/Client/Assets"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_app_bundle_stamp}"
    DEPENDS
        "${octaryn_client_shader_stage_stamp}"
        ${octaryn_client_shader_sources}
        ${octaryn_client_game_module_bundle_depends}
        ${octaryn_client_app_bundle_depends}
        octaryn_native_jobs
        "${octaryn_client_managed_STAMP}"
        "${octaryn_shared_STAMP}"
        ${octaryn_client_game_module_stamp_depends}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_command(
    OUTPUT "${octaryn_client_server_app_stamp}"
    BYPRODUCTS
        "${octaryn_client_server_dir}/Octaryn.Server.dll"
        "${octaryn_client_server_dir}/Octaryn.Server.deps.json"
        "${octaryn_client_server_dir}/Octaryn.Server.runtimeconfig.json"
        "${octaryn_client_server_dir}/Octaryn.Server${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_client_server_dir}/Octaryn.Shared.dll"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${octaryn_bundled_server_app_source_dir}"
        "${octaryn_client_server_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_server_app_stamp}"
    DEPENDS
        "${octaryn_client_app_bundle_stamp}"
        "${octaryn_bundled_server_app_source_stamp}"
        ${octaryn_bundled_server_app_source_target}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_client_server_app
    DEPENDS "${octaryn_client_server_app_stamp}")

add_custom_command(
    OUTPUT "${octaryn_client_bundle_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_bundle_stamp}"
    DEPENDS
        "${octaryn_client_server_app_stamp}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_client_bundle
    DEPENDS "${octaryn_client_bundle_stamp}")

add_dependencies(octaryn_client_bundle
    octaryn_client_server_app
    octaryn_client_native)
