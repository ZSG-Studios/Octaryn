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

set(octaryn_client_runtime_shader_stamp "${octaryn_client_shader_stage_stamp}")

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

include(Owners/ToolTargets/ToolValidationAggregates)
