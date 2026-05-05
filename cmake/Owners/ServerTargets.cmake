include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)

octaryn_owner_build_root(server_build_root server)
octaryn_owner_log_root(server_log_root server)
set(octaryn_server_bundle_dir "${server_build_root}/bundle")
set(octaryn_server_bundle_obj_dir "${server_build_root}/bundle-obj")
set(octaryn_server_bundle_stamp "${server_build_root}/stamps/octaryn_server_bundle.stamp")
set(octaryn_server_bundle_output "${octaryn_server_bundle_dir}/Octaryn.Server.dll")
set(octaryn_bundled_server_app_source_dir "${octaryn_server_bundle_dir}")
set(octaryn_bundled_server_app_source_target octaryn_server_bundle)
set(octaryn_bundled_server_app_source_stamp "${octaryn_server_bundle_stamp}")

octaryn_add_native_owner(octaryn_server_native)
add_dependencies(octaryn_server_native octaryn_shared_native)

octaryn_add_native_static_library(
    octaryn_server_world_time
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time/Clock.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time")

octaryn_add_native_shared_library(
    octaryn_server_block_store
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockCommandQueue.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockStoreApi.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockStore.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")

octaryn_add_native_shared_library(
    octaryn_server_player_simulation
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerSimulation.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players")

octaryn_add_native_shared_library(
    octaryn_server_terrain_generation
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation/TerrainGeneration.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation")

octaryn_add_native_shared_library(
    octaryn_server_world_persistence
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store")

add_dependencies(octaryn_server_native octaryn_server_world_time)
add_dependencies(octaryn_server_native octaryn_server_block_store)
add_dependencies(octaryn_server_native octaryn_server_player_simulation)
add_dependencies(octaryn_server_native octaryn_server_terrain_generation)
add_dependencies(octaryn_server_native octaryn_server_world_persistence)

if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    octaryn_add_native_shared_library(
        octaryn_server_managed_bridge
        server
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/HostBridge/NativeLoading/ManagedBridge.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/HostBridge/Abi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_shared_host_abi
            octaryn_native_diagnostics
            octaryn::dotnet_hosting)

    target_compile_definitions(octaryn_server_managed_bridge
        PRIVATE
            OCTARYN_SERVER_MANAGED_ASSEMBLY_PATH="${octaryn_server_bundle_dir}/Octaryn.Server.dll"
            OCTARYN_SERVER_RUNTIME_CONFIG_PATH="${octaryn_server_bundle_dir}/Octaryn.Server.runtimeconfig.json")

    add_dependencies(octaryn_server_native octaryn_server_managed_bridge)

    octaryn_add_native_executable(
        octaryn_server_launch_probe
        server
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/HostBridge/LaunchProbe/LaunchProbe.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/HostBridge/Abi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
        PRIVATE_LINKS
            octaryn_server_managed_bridge
            octaryn_native_diagnostics)

    target_compile_definitions(octaryn_server_launch_probe
        PRIVATE
            OCTARYN_SERVER_LAUNCH_PROBE_LOG_PATH="${server_log_root}/octaryn_server_launch_probe-${OCTARYN_BUILD_PRESET_NAME}.log")

    add_dependencies(octaryn_server_native octaryn_server_launch_probe)
else()
    add_custom_target(octaryn_server_managed_bridge
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server managed bridge: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
    add_custom_target(octaryn_server_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server launch probe binary: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()

octaryn_add_dotnet_owner(
    octaryn_server
    server
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Octaryn.Server.csproj")

add_dependencies(octaryn_server octaryn_shared)

set(octaryn_server_game_module_stamp_depends)
set(octaryn_server_game_module_bundle_depends)
set(octaryn_server_game_module_bundle_commands)
if(DEFINED octaryn_default_game_module_target)
    add_dependencies(octaryn_server "${octaryn_default_game_module_target}")
endif()
if(DEFINED octaryn_default_game_module_stamp)
    list(APPEND octaryn_server_game_module_stamp_depends
        "${octaryn_default_game_module_stamp}")
endif()
if(DEFINED octaryn_default_game_module_bundle_dir)
    list(APPEND octaryn_server_game_module_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${octaryn_default_game_module_bundle_dir}"
            "${octaryn_server_bundle_dir}")
endif()
if(DEFINED octaryn_default_game_module_bundle_target)
    list(APPEND octaryn_server_game_module_bundle_depends
        "${octaryn_default_game_module_bundle_target}")
endif()
if(DEFINED octaryn_default_game_module_bundle_stamp)
    list(APPEND octaryn_server_game_module_bundle_depends
        "${octaryn_default_game_module_bundle_stamp}")
endif()

add_custom_command(
    OUTPUT "${octaryn_server_STAMP}"
    APPEND
    DEPENDS
        "${octaryn_shared_STAMP}"
        ${octaryn_server_game_module_stamp_depends})

file(MAKE_DIRECTORY "${server_build_root}/stamps" "${server_log_root}")

add_custom_command(
    OUTPUT "${octaryn_server_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_server_bundle_output}"
        "${octaryn_server_bundle_dir}/Octaryn.Server.deps.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server.runtimeconfig.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.dll"
        "${octaryn_server_bundle_dir}/Octaryn.Server.pdb"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.pdb"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_server_bundle_dir}/Arch.dll"
        "${octaryn_server_bundle_dir}/Arch.EventBus.dll"
        "${octaryn_server_bundle_dir}/Arch.LowLevel.dll"
        "${octaryn_server_bundle_dir}/Arch.Relationships.dll"
        "${octaryn_server_bundle_dir}/Arch.System.dll"
        "${octaryn_server_bundle_dir}/Collections.Pooled.dll"
        "${octaryn_server_bundle_dir}/CommunityToolkit.HighPerformance.dll"
        "${octaryn_server_bundle_dir}/Microsoft.Extensions.ObjectPool.dll"
        "${octaryn_server_bundle_dir}/Schedulers.dll"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_server_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_server_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_server_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_server_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_server_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" restore "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Octaryn.Server.csproj"
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_server_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" publish "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Octaryn.Server.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --framework net10.0
        --output "${octaryn_server_bundle_dir}"
        --no-self-contained
        --no-restore
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${server_log_root}/octaryn_server_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    ${octaryn_server_game_module_bundle_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_native_jobs>"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_server_player_simulation>"
        "${octaryn_server_bundle_dir}/$<TARGET_FILE_NAME:octaryn_server_player_simulation>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_server_block_store>"
        "${octaryn_server_bundle_dir}/$<TARGET_FILE_NAME:octaryn_server_block_store>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_server_terrain_generation>"
        "${octaryn_server_bundle_dir}/$<TARGET_FILE_NAME:octaryn_server_terrain_generation>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_server_world_persistence>"
        "${octaryn_server_bundle_dir}/$<TARGET_FILE_NAME:octaryn_server_world_persistence>"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_server_bundle_stamp}"
    DEPENDS
        "${octaryn_server_STAMP}"
        octaryn_server_block_store
        octaryn_native_jobs
        octaryn_server_player_simulation
        octaryn_server_terrain_generation
        octaryn_server_world_persistence
        ${octaryn_server_game_module_bundle_depends}
        "${octaryn_shared_STAMP}"
        ${octaryn_server_game_module_stamp_depends}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_server_bundle
    DEPENDS "${octaryn_server_bundle_stamp}")

if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    set(octaryn_server_launch_probe_world_dir "${server_build_root}/launch-probe-world")
    set(octaryn_server_launch_probe_world_blocks "${octaryn_server_launch_probe_world_dir}/world_blocks.json")
    add_custom_target(octaryn_run_server_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_server_launch_probe_world_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_server_launch_probe_world_dir}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "OCTARYN_SERVER_WORLD_BLOCKS_PATH=${octaryn_server_launch_probe_world_blocks}"
            "OCTARYN_SERVER_PLAYER_SAVE_ROOT=${octaryn_server_launch_probe_world_dir}"
            "OCTARYN_NATIVE_JOBS_LIBRARY=$<TARGET_FILE:octaryn_native_jobs>"
            "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>"
            "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>"
            "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
            "$<TARGET_FILE:octaryn_server_launch_probe>"
        DEPENDS
            octaryn_server_bundle
            octaryn_native_jobs
            octaryn_server_block_store
            octaryn_server_terrain_generation
            octaryn_server_world_persistence
            octaryn_server_launch_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_run_server_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server launch probe: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()
