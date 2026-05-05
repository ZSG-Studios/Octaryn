cmake_minimum_required(VERSION 3.28)

project(Octaryn VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(OCTARYN_WORKSPACE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Octaryn workspace root")

include(Shared/ProjectDefaults)
include(Shared/CompilerWarnings)
include(Shared/TargetArchitecture)
include(Shared/BuildOutputs)
include(Shared/OwnerBuildLayout)
include(Platforms/PlatformDispatch)

octaryn_setup_project_defaults()

include(Dependencies/DependencyPolicy)
include(Dependencies/NativeDependencyAliases)
include(Owners/SharedTargets)
include(Owners/BasegameTargets)
include(Owners/ServerTargets)
include(Owners/ClientTargets)
include(Owners/ToolTargets)

if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux" AND OCTARYN_TARGET_ARCH STREQUAL "x64")
    set(octaryn_client_runtime_shader_stamp
        "${client_build_root}/stamps/octaryn_client_runtime_shaders.stamp")
    add_custom_command(
        OUTPUT "${octaryn_client_runtime_shader_stamp}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${octaryn_client_bundle_dir}/Client/Shaders/Compiled"
        COMMAND "$<TARGET_FILE:octaryn_shader_compiler>"
            --output-dir "${octaryn_client_bundle_dir}/Client/Shaders/Compiled"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sky.vert.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sky.frag.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/opaque_packed.vert.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sprite_packed.vert.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/opaque.frag.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/present.vert.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/present.frag.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/composite.comp.glsl"
            --input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/ui.comp.glsl"
        COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_runtime_shader_stamp}"
        DEPENDS
            octaryn_shader_compiler
            "${octaryn_client_app_bundle_stamp}"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sky.vert.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sky.frag.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/opaque_packed.vert.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/sprite_packed.vert.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/opaque.frag.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/present.vert.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/present.frag.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/composite.comp.glsl"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/ui.comp.glsl"
            ${octaryn_client_shader_sources}
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${octaryn_client_bundle_stamp}"
        APPEND
        DEPENDS "${octaryn_client_runtime_shader_stamp}")
endif()

add_custom_target(octaryn_all
    DEPENDS
        octaryn_shared
        octaryn_basegame
        octaryn_server
        octaryn_server_native
        octaryn_server_bundle
        octaryn_client_native
        octaryn_client_bundle
        octaryn_tools)

add_custom_target(octaryn_validate_all
    DEPENDS
        octaryn_validate_cmake_targets
        octaryn_validate_cmake_policy_separation
        octaryn_validate_cmake_dependency_aliases
        octaryn_validate_package_policy_sync
        octaryn_validate_project_references
        octaryn_validate_module_manifest_packages
        octaryn_validate_module_manifest_files
        octaryn_validate_module_manifest_probe
        octaryn_validate_bundle_module_payload
        octaryn_validate_client_server_app
        octaryn_client_server_app_launch_probe
        octaryn_validate_client_shader_bundle
        octaryn_validate_client_app_launch_probe
        octaryn_validate_module_source_api
        octaryn_validate_module_binary_sandbox
        octaryn_validate_module_layout
        octaryn_validate_basegame_block_catalog
        octaryn_validate_basegame_worldgen_content
        octaryn_validate_dotnet_package_assets
        octaryn_validate_native_abi_contracts
        octaryn_validate_native_owner_boundaries
        octaryn_validate_native_archive_format
        octaryn_validate_native_jobs_probe
        octaryn_validate_client_chunk_mesh_plan_probe
        octaryn_validate_client_empty_world_mesh_probe
        octaryn_validate_dotnet_owners
        octaryn_validate_world_time_probe
        octaryn_validate_server_world_time_native_probe
        octaryn_validate_server_block_store_native_probe
        octaryn_validate_server_player_simulation_native_probe
        octaryn_validate_server_persistence_probe
        octaryn_validate_server_world_blocks_probe
        octaryn_validate_server_world_generation_probe
        octaryn_validate_basegame_player_probe
        octaryn_validate_basegame_interaction_probe
        octaryn_validate_owner_module_validation_probe
        octaryn_validate_hostfxr_bridge_exports
        octaryn_validate_owner_launch_probes)
