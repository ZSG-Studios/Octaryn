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

if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
    octaryn_add_native_shared_library(
        octaryn_server_managed_bridge
        server
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Native/ManagedBridge/octaryn_server_managed_bridge.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Native/ServerHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
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
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Native/LaunchProbe/octaryn_server_launch_probe.c"
        PUBLIC_INCLUDE_DIRS
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Native/ServerHostAbi"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Native/HostAbi"
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

add_dependencies(octaryn_server octaryn_shared octaryn_basegame)

add_custom_command(
    OUTPUT "${octaryn_server_STAMP}"
    APPEND
    DEPENDS
        "${octaryn_shared_STAMP}"
        "${octaryn_basegame_STAMP}")

file(MAKE_DIRECTORY "${server_build_root}/stamps" "${server_log_root}")

add_custom_command(
    OUTPUT "${octaryn_server_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_server_bundle_output}"
        "${octaryn_server_bundle_dir}/Octaryn.Server.deps.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server.runtimeconfig.json"
        "${octaryn_server_bundle_dir}/Octaryn.Server${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_server_bundle_dir}/Octaryn.Basegame.dll"
        "${octaryn_server_bundle_dir}/Octaryn.Basegame.deps.json"
        "${octaryn_server_bundle_dir}/Octaryn.Basegame.runtimeconfig.json"
        "${octaryn_server_bundle_dir}/Data/Module/octaryn.basegame.module.json"
        "${octaryn_server_bundle_dir}/Data/Blocks/octaryn.basegame.blocks.json"
        "${octaryn_server_bundle_dir}/Data/Biomes/octaryn.basegame.biomes.json"
        "${octaryn_server_bundle_dir}/Data/Features/octaryn.basegame.features.json"
        "${octaryn_server_bundle_dir}/Data/Items/octaryn.basegame.item.hand.json"
        "${octaryn_server_bundle_dir}/Data/Rules/octaryn.basegame.rule.default_interaction.json"
        "${octaryn_server_bundle_dir}/Data/Rules/octaryn.basegame.rule.terrain_generation.json"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.dll"
        "${octaryn_server_bundle_dir}/Octaryn.Server.pdb"
        "${octaryn_server_bundle_dir}/Octaryn.Basegame.pdb"
        "${octaryn_server_bundle_dir}/Octaryn.Shared.pdb"
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
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${octaryn_basegame_bundle_dir}"
        "${octaryn_server_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_server_bundle_stamp}"
    DEPENDS
        "${octaryn_server_STAMP}"
        octaryn_basegame_bundle
        "${octaryn_shared_STAMP}"
        "${octaryn_basegame_STAMP}"
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
            "$<TARGET_FILE:octaryn_server_launch_probe>"
        DEPENDS
            octaryn_server_bundle
            octaryn_server_launch_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_run_server_launch_probe
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping server launch probe: .NET native hosting unavailable for ${OCTARYN_TARGET_PLATFORM}."
        VERBATIM)
endif()
