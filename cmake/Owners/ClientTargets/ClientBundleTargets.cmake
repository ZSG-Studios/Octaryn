set(octaryn_client_bundle_stage_dir "${octaryn_client_bundle_dir}.staging")
set(octaryn_client_bundle_installer "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/support/install_bundle.py")
include(Owners/ClientTargets/ClientRuntimeDllTargets)

file(GLOB_RECURSE octaryn_client_asset_sources CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/*")
list(FILTER octaryn_client_asset_sources EXCLUDE REGEX "/\\.gitkeep$")

file(MAKE_DIRECTORY "${client_build_root}/stamps" "${client_log_root}")

add_custom_command(
    OUTPUT "${octaryn_client_app_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_client_bundle_dir}/Octaryn.Client${CMAKE_EXECUTABLE_SUFFIX}"
        "${octaryn_client_bundle_dir}/Licenses/RmlUi.txt"
        "${octaryn_client_bundle_dir}/Licenses/OpenALSoft.txt"
        "${octaryn_client_bundle_dir}/Licenses/miniaudio.txt"
        "${octaryn_client_bundle_dir}/Licenses/Silkscreen.txt"
        ${octaryn_client_runtime_bundle_outputs}
        ${octaryn_client_shader_bundle_outputs}
        "${octaryn_client_bundle_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
    COMMAND "${Python3_EXECUTABLE}" "${octaryn_client_bundle_installer}"
        prepare --bundle "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE:octaryn_client_app>"
        "${octaryn_client_bundle_stage_dir}/Octaryn.Client${CMAKE_EXECUTABLE_SUFFIX}"
    ${octaryn_client_runtime_bundle_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_native_jobs>"
        "${octaryn_client_bundle_stage_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}octaryn_native_jobs${CMAKE_SHARED_LIBRARY_SUFFIX}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${octaryn_client_shader_stage_dir}"
        "${octaryn_client_bundle_stage_dir}/Client/Shaders"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets"
        "${octaryn_client_bundle_stage_dir}/Client/Assets"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_bundle_stage_dir}/Licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_RMLUI_SOURCE_DIR}/LICENSE.txt"
        "${octaryn_client_bundle_stage_dir}/Licenses/RmlUi.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OpenAL_SOURCE_DIR}/COPYING"
        "${octaryn_client_bundle_stage_dir}/Licenses/OpenALSoft.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${miniaudio_source_dir}/LICENSE"
        "${octaryn_client_bundle_stage_dir}/Licenses/miniaudio.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Ui/Fonts/OFL.txt"
        "${octaryn_client_bundle_stage_dir}/Licenses/Silkscreen.txt"
    COMMAND "${Python3_EXECUTABLE}" "${octaryn_client_bundle_installer}"
        install --bundle "${octaryn_client_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_app_bundle_stamp}"
    DEPENDS
        "${octaryn_client_bundle_installer}"
        "${octaryn_client_shader_stage_stamp}"
        ${octaryn_client_shader_sources}
        ${octaryn_client_asset_sources}
        "${OCTARYN_RMLUI_SOURCE_DIR}/LICENSE.txt"
        "${OpenAL_SOURCE_DIR}/COPYING"
        "${miniaudio_source_dir}/LICENSE"
        ${octaryn_client_runtime_files}
        octaryn_client_app
        octaryn_native_jobs
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_command(
    OUTPUT "${octaryn_client_bundle_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_bundle_stamp}"
    DEPENDS
        "${octaryn_client_app_bundle_stamp}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_client_bundle
    DEPENDS "${octaryn_client_bundle_stamp}")

add_dependencies(octaryn_client_bundle
    octaryn_client_native)
