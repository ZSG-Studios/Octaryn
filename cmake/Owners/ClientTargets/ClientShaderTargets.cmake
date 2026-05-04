file(GLOB_RECURSE octaryn_client_shader_sources CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/*")
list(FILTER octaryn_client_shader_sources EXCLUDE REGEX "/\\.gitkeep$")
set(octaryn_client_shader_bundle_outputs)
foreach(octaryn_client_shader_source IN LISTS octaryn_client_shader_sources)
    file(RELATIVE_PATH octaryn_client_shader_file
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_source}")
    list(APPEND octaryn_client_shader_bundle_outputs
        "${octaryn_client_bundle_dir}/Client/Shaders/${octaryn_client_shader_file}")
endforeach()

add_custom_command(
    OUTPUT "${octaryn_client_shader_stage_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_shader_stage_stamp}"
    DEPENDS ${octaryn_client_shader_sources}
    VERBATIM)

add_custom_target(octaryn_client_shaders
    DEPENDS "${octaryn_client_shader_stage_stamp}")
