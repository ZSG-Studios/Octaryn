include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)

octaryn_owner_build_root(server_build_root server)
octaryn_owner_log_root(server_log_root server)
set(octaryn_server_bundle_dir "${server_build_root}/bundle")
set(octaryn_server_bundle_stage "${octaryn_server_bundle_dir}.staging")
set(octaryn_server_bundle_installer "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/support/install_bundle.py")
set(octaryn_server_bundle_obj_dir "${server_build_root}/bundle-obj")
set(octaryn_server_bundle_stamp "${server_build_root}/stamps/octaryn_server_bundle.stamp")
set(octaryn_server_bundle_output "${octaryn_server_bundle_dir}/Octaryn.Server.dll")
set(octaryn_bundled_server_app_source_dir "${octaryn_server_bundle_dir}")
set(octaryn_bundled_server_app_source_target octaryn_server_bundle)
set(octaryn_bundled_server_app_source_stamp "${octaryn_server_bundle_stamp}")

octaryn_add_native_owner(octaryn_server_native)
add_dependencies(octaryn_server_native octaryn_shared_native)

octaryn_add_native_shared_library(
    octaryn_server_host
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Host/HostPolicy.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Host"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")

include(Owners/NativeWorldTimeTargets)

octaryn_add_native_shared_library(
    octaryn_server_authority_tick
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Tick/AuthorityTick.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Tick"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs"
    PRIVATE_LINKS
        octaryn_native_jobs)

add_library(octaryn_server_block_store_objects OBJECT
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Fluids/FluidScheduler.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Fluids/FluidSimulation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Fluids/FluidEvaluator.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Fluids/FluidSlope.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockCommandQueueApi.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockCommandQueue.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockChangeQueue.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockEditService.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockStoreApi.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store/BlockStore.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Commands/Interaction/BlockInteractionFrameTracker.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Commands/Interaction/BlockInteractionIntent.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkColumnRequest.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkColumnStream.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkStreamBinarySnapshot.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkViewIntent.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming/ChunkStreamWriteTracker.cpp")
set(octaryn_server_block_store_includes
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Fluids"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")
octaryn_apply_owner_layout(octaryn_server_block_store_objects server)
octaryn_enable_default_warnings(octaryn_server_block_store_objects)
set_target_properties(octaryn_server_block_store_objects PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(octaryn_server_block_store_objects
    PRIVATE ${octaryn_server_block_store_includes})
target_link_libraries(octaryn_server_block_store_objects PRIVATE octaryn::deps::glaze)

# Internal C++ probes reuse the same objects without exporting STL classes.
octaryn_add_native_static_library(
    octaryn_server_block_store_internal
    server
    SOURCES $<TARGET_OBJECTS:octaryn_server_block_store_objects>
    PUBLIC_INCLUDE_DIRS ${octaryn_server_block_store_includes}
    PRIVATE_LINKS octaryn::deps::glaze)

octaryn_add_native_shared_library(
    octaryn_server_block_store
    server
    SOURCES $<TARGET_OBJECTS:octaryn_server_block_store_objects>
    PUBLIC_INCLUDE_DIRS ${octaryn_server_block_store_includes}
    PRIVATE_LINKS
        octaryn::deps::glaze)

octaryn_add_native_shared_library(
    octaryn_server_player_simulation
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerInputIntent.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerInput.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerStateLoad.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerJoltMovement.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerJoltWorld.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerPlacementPolicy.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerMovement.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players/PlayerSimulation.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
    PRIVATE_LINKS
        octaryn::deps::glaze
        octaryn::deps::jolt
        octaryn_server_block_store)
target_include_directories(octaryn_server_player_simulation
    PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store")
include("${CMAKE_CURRENT_LIST_DIR}/WorldItemsTargets.cmake")

octaryn_add_native_shared_library(
    octaryn_server_terrain_generation
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation/TerrainGeneration.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation"
    PRIVATE_LINKS
        octaryn_server_block_store)

target_include_directories(octaryn_server_terrain_generation PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain")

octaryn_add_native_shared_library(
    octaryn_server_world_persistence
    server
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/PlayerPersistence.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/PlayerDirectory.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/PathPolicy.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/ChunkOverrideDirectory.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/ChunkOverridePersistence.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldMetadataPersistence.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldGenerationPersistence.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldTimePersistence.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldPersistenceGzip.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldSaveImport.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/SaveExportBundle.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks/WorldPersistence.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
    PRIVATE_LINKS
        octaryn::deps::glaze
        octaryn::deps::zlib)

add_dependencies(octaryn_server_native octaryn_server_host)
add_dependencies(octaryn_server_native octaryn_server_world_time)
add_dependencies(octaryn_server_native octaryn_server_authority_tick)
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

    octaryn_stage_dotnet_host_runtime(octaryn_server_managed_bridge)

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
    "${octaryn_server_bundle_stage}")
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

set(octaryn_server_runtime_bundle_commands)
set(octaryn_server_runtime_bundle_outputs)
if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    list(APPEND octaryn_server_runtime_bundle_outputs "${octaryn_server_bundle_dir}/${OCTARYN_DOTNET_NETHOST_RUNTIME_NAME}")
    list(APPEND octaryn_server_runtime_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${OCTARYN_DOTNET_NETHOST_RUNTIME}" "${octaryn_server_bundle_stage}/${OCTARYN_DOTNET_NETHOST_RUNTIME_NAME}")
endif()

add_custom_command(
    OUTPUT "${octaryn_server_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_server_bundle_output}"
        ${octaryn_server_runtime_bundle_outputs}
        "${octaryn_server_bundle_dir}/Octaryn.Server.deps.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server.runtimeconfig.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.dll"
        "${octaryn_server_bundle_dir}/Octaryn.Server.pdb"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.pdb"
        "${octaryn_server_bundle_dir}/LiteNetLib.dll"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_server_host${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_server_world_time${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_server_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_server_authority_tick${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${octaryn_server_bundle_dir}/Arch.dll"
        "${octaryn_server_bundle_dir}/Arch.EventBus.dll"
        "${octaryn_server_bundle_dir}/Arch.LowLevel.dll"
        "${octaryn_server_bundle_dir}/Arch.Relationships.dll"
        "${octaryn_server_bundle_dir}/Arch.System.dll"
        "${octaryn_server_bundle_dir}/Collections.Pooled.dll"
        "${octaryn_server_bundle_dir}/CommunityToolkit.HighPerformance.dll"
        "${octaryn_server_bundle_dir}/Microsoft.Extensions.ObjectPool.dll"
        "${octaryn_server_bundle_dir}/Schedulers.dll"
  COMMAND "${Python3_EXECUTABLE}" "${octaryn_server_bundle_installer}"
    prepare --bundle "${octaryn_server_bundle_dir}"
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
    --output "${octaryn_server_bundle_stage}"
        --no-self-contained
        --no-restore
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${server_log_root}/octaryn_server_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    ${octaryn_server_game_module_bundle_commands}
    ${octaryn_server_runtime_bundle_commands}
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_native_jobs>"
    "${octaryn_server_bundle_stage}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_host>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_host>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_world_time>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_world_time>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_authority_tick>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_authority_tick>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_player_simulation>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_player_simulation>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_world_items>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_world_items>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_block_store>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_block_store>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_terrain_generation>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_terrain_generation>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:octaryn_server_world_persistence>"
    "${octaryn_server_bundle_stage}/$<TARGET_FILE_NAME:octaryn_server_world_persistence>"
  COMMAND "${Python3_EXECUTABLE}" "${octaryn_server_bundle_installer}"
    install --bundle "${octaryn_server_bundle_dir}" --preserve-directory octaryn-world
  COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_server_bundle_stamp}"
  DEPENDS
    "${octaryn_server_bundle_installer}"
        "${octaryn_server_STAMP}"
        octaryn_server_host
        octaryn_server_world_time
        octaryn_server_authority_tick
        octaryn_server_block_store
        octaryn_native_jobs
        octaryn_server_player_simulation
        octaryn_server_world_items
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
            "OCTARYN_SERVER_HOST_LIBRARY=$<TARGET_FILE:octaryn_server_host>"
            "OCTARYN_SERVER_WORLD_TIME_LIBRARY=$<TARGET_FILE:octaryn_server_world_time>"
            "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY=$<TARGET_FILE:octaryn_server_authority_tick>"
            "OCTARYN_SERVER_BLOCK_STORE_LIBRARY=$<TARGET_FILE:octaryn_server_block_store>"
            "OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY=$<TARGET_FILE:octaryn_server_terrain_generation>"
            "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=$<TARGET_FILE:octaryn_server_world_persistence>"
            "OCTARYN_SERVER_MANAGED_ASSEMBLY_PATH=${octaryn_server_bundle_dir}/Octaryn.Server.dll"
            "OCTARYN_SERVER_RUNTIME_CONFIG_PATH=${octaryn_server_bundle_dir}/Octaryn.Server.runtimeconfig.json"
            "$<TARGET_FILE:octaryn_server_launch_probe>"
        DEPENDS
            octaryn_server_bundle
            octaryn_server_host
            octaryn_native_jobs
            octaryn_server_world_time
            octaryn_server_authority_tick
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
