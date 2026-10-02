include_guard(GLOBAL)

set(OCTARYN_GAME_PROJECT "" CACHE FILEPATH "External game .csproj; empty selects Octaryn Basegame")
if(NOT OCTARYN_GAME_PROJECT)
    include(Owners/BasegameTargets)
    return()
endif()

get_filename_component(OCTARYN_GAME_PROJECT "${OCTARYN_GAME_PROJECT}" ABSOLUTE)
if(NOT EXISTS "${OCTARYN_GAME_PROJECT}")
    message(FATAL_ERROR "Game project not found: ${OCTARYN_GAME_PROJECT}")
endif()
include(Owners/DotNetOwner)
get_filename_component(game_source_dir "${OCTARYN_GAME_PROJECT}" DIRECTORY)
get_filename_component(game_assembly "${OCTARYN_GAME_PROJECT}" NAME_WE)
octaryn_add_dotnet_owner(octaryn_game game "${OCTARYN_GAME_PROJECT}")
add_dependencies(octaryn_game octaryn_shared)
file(GLOB game_library_projects CONFIGURE_DEPENDS "${game_source_dir}/Libraries/*.csproj")
set(game_library_stamps)
foreach(game_library_project IN LISTS game_library_projects)
    get_filename_component(game_library_name "${game_library_project}" NAME_WE)
    string(MAKE_C_IDENTIFIER "octaryn_game_library_${game_library_name}" game_library_target)
    octaryn_add_dotnet_owner(${game_library_target} game "${game_library_project}")
    add_dependencies(${game_library_target} octaryn_shared)
    file(GLOB_RECURSE game_library_sources CONFIGURE_DEPENDS "${game_source_dir}/Source/Libraries/*.cs")
    add_custom_command(OUTPUT "${${game_library_target}_STAMP}" APPEND
        DEPENDS "${octaryn_shared_STAMP}" ${game_library_sources})
    add_dependencies(octaryn_game ${game_library_target})
    list(APPEND game_library_stamps "${${game_library_target}_STAMP}")
endforeach()
if(game_library_stamps)
    add_custom_command(OUTPUT "${octaryn_game_STAMP}" APPEND DEPENDS ${game_library_stamps})
endif()
octaryn_owner_build_root(game_build_root game)
octaryn_owner_log_root(game_log_root game)
set(game_bundle_dir "${game_build_root}/bundle")
set(game_bundle_stage "${game_bundle_dir}.staging")
set(game_bundle_installer "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/support/install_bundle.py")
set(game_bundle_stamp "${game_build_root}/stamps/octaryn_game_bundle.stamp")
set(OCTARYN_GAME_CONTENT_ROOT "" CACHE PATH "Locally prepared game Data/Content and Assets package")
set(game_package_commands)
if(OCTARYN_GAME_CONTENT_ROOT)
    if(NOT IS_DIRECTORY "${OCTARYN_GAME_CONTENT_ROOT}/Data/Content" OR
       NOT IS_DIRECTORY "${OCTARYN_GAME_CONTENT_ROOT}/Assets")
        message(FATAL_ERROR "Game content package requires Data/Content and Assets: ${OCTARYN_GAME_CONTENT_ROOT}")
    endif()
    list(APPEND game_package_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OCTARYN_GAME_CONTENT_ROOT}/Data" "${game_bundle_stage}/Data"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OCTARYN_GAME_CONTENT_ROOT}/Assets" "${game_bundle_stage}/Assets")
endif()
file(GLOB_RECURSE game_content_sources CONFIGURE_DEPENDS
    "${game_source_dir}/Data/*" "${game_source_dir}/Assets/*")
set(game_build_metadata)
foreach(game_metadata IN ITEMS Directory.Build.props Directory.Build.targets Directory.Packages.props)
    if(EXISTS "${game_source_dir}/${game_metadata}")
        list(APPEND game_build_metadata "${game_source_dir}/${game_metadata}")
    endif()
endforeach()
if(game_build_metadata)
    add_custom_command(OUTPUT "${octaryn_game_STAMP}" APPEND DEPENDS ${game_build_metadata})
endif()
if(OCTARYN_GAME_CONTENT_ROOT)
    file(GLOB_RECURSE game_package_sources CONFIGURE_DEPENDS
        "${OCTARYN_GAME_CONTENT_ROOT}/Data/*" "${OCTARYN_GAME_CONTENT_ROOT}/Assets/*")
    list(APPEND game_content_sources ${game_package_sources})
endif()
add_custom_command(
    OUTPUT "${game_bundle_stamp}"
    BYPRODUCTS "${game_bundle_dir}/${game_assembly}.dll"
    COMMAND "${Python3_EXECUTABLE}" "${game_bundle_installer}" prepare --bundle "${game_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynEngineRoot=${OCTARYN_WORKSPACE_ROOT_DIR}"
        "${DOTNET_EXECUTABLE}" publish "${OCTARYN_GAME_PROJECT}"
        --configuration "${CMAKE_BUILD_TYPE}" --framework net10.0
        --output "${game_bundle_stage}" --no-self-contained
        -p:OctarynSkipModuleResolvedReferenceValidation=true
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${game_log_root}/octaryn_game_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    ${game_package_commands}
    COMMAND "${Python3_EXECUTABLE}" "${game_bundle_installer}" install --bundle "${game_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${game_bundle_stamp}"
    DEPENDS "${octaryn_game_STAMP}" "${octaryn_shared_STAMP}" "${game_bundle_installer}"
        ${game_content_sources} ${game_build_metadata}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)
add_custom_target(octaryn_game_bundle DEPENDS "${game_bundle_stamp}")
set(octaryn_default_game_module_target octaryn_game)
set(octaryn_default_game_module_stamp "${octaryn_game_STAMP}")
set(octaryn_default_game_module_bundle_target octaryn_game_bundle)
set(octaryn_default_game_module_bundle_stamp "${game_bundle_stamp}")
set(octaryn_default_game_module_bundle_dir "${game_bundle_dir}")
add_custom_command(OUTPUT "${octaryn_game_STAMP}" APPEND DEPENDS "${octaryn_shared_STAMP}")
message(STATUS "Selected game project: ${OCTARYN_GAME_PROJECT}")
