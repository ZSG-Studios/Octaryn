if(OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT)
    add_custom_target(octaryn_validate_native_archive_format
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_native_archive_format.py"
            --archive "${OCTARYN_BUILD_PRESET_ROOT}/shared/native/lib/liboctaryn_shared_host_abi.a"
            --expected-format "${OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT}"
            --objdump "${OCTARYN_TARGET_OBJDUMP}"
        DEPENDS
            octaryn_shared_host_abi
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_validate_native_archive_format
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping native archive format validation: no target archive format declared for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()

if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux" AND OCTARYN_TARGET_ARCH STREQUAL "x64")
    add_custom_target(octaryn_validate_native_jobs_probe
        COMMAND "$<TARGET_FILE:octaryn_native_jobs_probe>"
        DEPENDS
            octaryn_native_jobs_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_client_chunk_mesh_plan_probe
        COMMAND "$<TARGET_FILE:octaryn_client_chunk_mesh_plan_probe>"
        DEPENDS
            octaryn_client_chunk_mesh_plan_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_client_empty_world_mesh_probe
        COMMAND "$<TARGET_FILE:octaryn_client_empty_world_mesh_probe>"
        DEPENDS
            octaryn_client_empty_world_mesh_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_host_policy_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_host_policy_probe>"
        DEPENDS
            octaryn_server_host_policy_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_world_time_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_world_time_probe>"
        DEPENDS
            octaryn_server_world_time_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_authority_tick_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_authority_tick_probe>"
        DEPENDS
            octaryn_server_authority_tick_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_block_store_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_block_store_probe>"
        DEPENDS
            octaryn_server_block_store_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_player_simulation_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_player_simulation_probe>"
        DEPENDS
            octaryn_server_player_simulation_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_terrain_generation_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_terrain_generation_probe>"
        DEPENDS
            octaryn_server_terrain_generation_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_target(octaryn_validate_server_world_persistence_native_probe
        COMMAND "$<TARGET_FILE:octaryn_server_world_persistence_probe>"
        DEPENDS
            octaryn_server_world_persistence_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_validate_native_jobs_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping native jobs probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_client_chunk_mesh_plan_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client chunk mesh plan probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_client_empty_world_mesh_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping client empty world mesh probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_host_policy_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server host policy native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_world_time_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server world time native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_authority_tick_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server authority tick native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_block_store_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server block store native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_player_simulation_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server player simulation native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_terrain_generation_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server terrain generation native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
    add_custom_target(octaryn_validate_server_world_persistence_native_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server world persistence native probe: native probe host execution is only active for Linux/x64 targets."
        VERBATIM)
endif()

add_custom_target(octaryn_validate_dotnet_owners
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/Octaryn.DotNet.sln"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" build
        "${OCTARYN_WORKSPACE_ROOT_DIR}/Octaryn.DotNet.sln"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    DEPENDS
        octaryn_shared
        octaryn_basegame
        octaryn_client_managed
        octaryn_server
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_dependencies(octaryn_validate_dotnet_owners
    octaryn_client_bundle
    octaryn_server_bundle)

add_custom_target(octaryn_validate_world_time_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.WorldTimeProbe/Octaryn.WorldTimeProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OCTARYN_WORLD_TIME_PROBE_DIR=${octaryn_debug_tool_root}/world-time-probe" "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>" "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.WorldTimeProbe/Octaryn.WorldTimeProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    DEPENDS
        octaryn_server_world_time
        octaryn_server_world_persistence
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_server_world_blocks_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerWorldBlocksProbe/Octaryn.ServerWorldBlocksProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR=${tool_server_build_root}/validation/server-world-blocks" "OCTARYN_SERVER_WORLD_BLOCKS_PATH=${tool_server_build_root}/validation/server-world-blocks/world_blocks.json" "OCTARYN_NATIVE_JOBS_LIBRARY=$<TARGET_FILE:octaryn_native_jobs>" "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>" "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY=$<TARGET_FILE:octaryn_server_authority_tick>" "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>" "OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY=$<TARGET_FILE:octaryn_server_player_simulation>" "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>" "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerWorldBlocksProbe/Octaryn.ServerWorldBlocksProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    DEPENDS
        octaryn_native_jobs
        octaryn_server_world_time
        octaryn_server_authority_tick
        octaryn_server_block_store
        octaryn_server_player_simulation
        octaryn_server_terrain_generation
        octaryn_server_world_persistence
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_server_persistence_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerPersistenceProbe/Octaryn.ServerPersistenceProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OCTARYN_SERVER_PERSISTENCE_PROBE_DIR=${tool_server_build_root}/validation/server-persistence" "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>" "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerPersistenceProbe/Octaryn.ServerPersistenceProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
add_dependencies(octaryn_validate_server_persistence_probe
    octaryn_server_block_store
    octaryn_server_world_persistence)

add_custom_target(octaryn_validate_server_world_generation_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerWorldGenerationProbe/Octaryn.ServerWorldGenerationProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OCTARYN_NATIVE_JOBS_LIBRARY=$<TARGET_FILE:octaryn_native_jobs>" "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>" "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY=$<TARGET_FILE:octaryn_server_authority_tick>" "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>" "OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY=$<TARGET_FILE:octaryn_server_player_simulation>" "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>" "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ServerWorldGenerationProbe/Octaryn.ServerWorldGenerationProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    DEPENDS
        octaryn_native_jobs
        octaryn_server_world_time
        octaryn_server_authority_tick
        octaryn_server_block_store
        octaryn_server_player_simulation
        octaryn_server_terrain_generation
        octaryn_server_world_persistence
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_basegame_player_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.BasegamePlayerProbe/Octaryn.BasegamePlayerProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.BasegamePlayerProbe/Octaryn.BasegamePlayerProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_basegame_interaction_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.BasegameInteractionProbe/Octaryn.BasegameInteractionProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.BasegameInteractionProbe/Octaryn.BasegameInteractionProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_owner_module_validation_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.OwnerModuleValidationProbe/Octaryn.OwnerModuleValidationProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OCTARYN_NATIVE_JOBS_LIBRARY=$<TARGET_FILE:octaryn_native_jobs>" "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>" "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY=$<TARGET_FILE:octaryn_server_authority_tick>" "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>" "OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY=$<TARGET_FILE:octaryn_server_player_simulation>" "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>" "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.OwnerModuleValidationProbe/Octaryn.OwnerModuleValidationProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
    DEPENDS
        octaryn_native_jobs
        octaryn_server_world_time
        octaryn_server_authority_tick
        octaryn_server_block_store
        octaryn_server_player_simulation
        octaryn_server_terrain_generation
        octaryn_server_world_persistence
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_dependencies(octaryn_validate_module_manifest_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_module_source_api octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_module_binary_sandbox octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_world_time_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_server_persistence_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_server_world_blocks_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_server_world_generation_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_basegame_player_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_basegame_interaction_probe octaryn_validate_dotnet_owners)
add_dependencies(octaryn_validate_owner_module_validation_probe octaryn_validate_dotnet_owners)

if(OCTARYN_DOTNET_HOSTING_AVAILABLE AND OCTARYN_TARGET_PLATFORM STREQUAL "Linux" AND OCTARYN_TARGET_ARCH STREQUAL "x64")
    add_custom_target(octaryn_validate_hostfxr_bridge_exports
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_hostfxr_bridge_exports.py"
            --owner client
            --bundle-dir "${octaryn_tool_client_bundle_dir}"
            --bridge "$<TARGET_FILE:octaryn_client_managed_bridge>"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_hostfxr_bridge_exports.py"
            --owner server
            --bundle-dir "${octaryn_tool_server_bundle_dir}"
            --bridge "$<TARGET_FILE:octaryn_server_managed_bridge>"
        DEPENDS
            "${octaryn_tool_client_bundle_output}"
            "${octaryn_tool_client_bundle_runtime_config}"
            "${octaryn_tool_client_bundle_deps}"
            "${octaryn_tool_server_bundle_output}"
            "${octaryn_tool_server_bundle_runtime_config}"
            "${octaryn_tool_server_bundle_deps}"
            octaryn_client_bundle
            octaryn_server_bundle
            octaryn_client_managed_bridge
            octaryn_server_managed_bridge
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)

    add_custom_target(octaryn_validate_owner_launch_probes
        COMMAND "${CMAKE_COMMAND}" -E env
            "OCTARYN_CLIENT_BLOCK_CATALOG_PATH=${octaryn_tool_client_bundle_dir}/Data/Blocks/octaryn.basegame.blocks.json"
            "$<TARGET_FILE:octaryn_client_launch_probe>"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_tool_server_probe_world_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_tool_server_probe_world_dir}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${octaryn_tool_server_live_debug_probe_log}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "OCTARYN_SERVER_WORLD_BLOCKS_PATH=${octaryn_tool_server_probe_world_blocks}"
            "OCTARYN_SERVER_PLAYER_SAVE_ROOT=${octaryn_tool_server_probe_world_dir}"
            "OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH=${octaryn_tool_server_live_debug_probe_log}"
            "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>"
            "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY=$<TARGET_FILE:octaryn_server_authority_tick>"
            "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>"
            "OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY=$<TARGET_FILE:octaryn_server_player_simulation>"
            "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>"
            "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
            "$<TARGET_FILE:octaryn_server_launch_probe>"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_owner_launch_probe_logs.py"
            --owner client
            --log-file "${octaryn_tool_client_probe_log}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_owner_launch_probe_logs.py"
            --owner server
            --log-file "${octaryn_tool_server_probe_log}"
        COMMAND python3
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_server_live_debug_probe_log.py"
            --log-file "${octaryn_tool_server_live_debug_probe_log}"
        DEPENDS
            "${octaryn_tool_client_bundle_output}"
            "${octaryn_tool_client_bundle_runtime_config}"
            "${octaryn_tool_server_bundle_output}"
            "${octaryn_tool_server_bundle_runtime_config}"
            octaryn_client_bundle
            octaryn_server_bundle
            octaryn_server_world_time
            octaryn_server_authority_tick
            octaryn_server_block_store
            octaryn_server_terrain_generation
            octaryn_server_world_persistence
            octaryn_client_launch_probe
            octaryn_server_launch_probe
            octaryn_validate_hostfxr_bridge_exports
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_validate_hostfxr_bridge_exports
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping hostfxr bridge export validation: bridge host execution is only active for Linux/x64 targets with .NET native hosting."
        VERBATIM)

    add_custom_target(octaryn_validate_owner_launch_probes
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping owner launch probes: owner launch probe host execution is only active for Linux/x64 targets with .NET native hosting."
        VERBATIM)
endif()
