file(GLOB_RECURSE octaryn_client_shader_sources CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/*")
list(FILTER octaryn_client_shader_sources EXCLUDE REGEX "/\\.gitkeep$")
file(GLOB_RECURSE octaryn_fsr2_vendor_sources CONFIGURE_DEPENDS
    "${OCTARYN_FSR2_SHADER_VENDOR}/*")
set(octaryn_client_shader_bundle_outputs)
foreach(octaryn_client_shader_source IN LISTS octaryn_client_shader_sources)
    if(NOT octaryn_client_shader_source MATCHES "\\.slang$")
        message(FATAL_ERROR "Client shader sources must be Slang: ${octaryn_client_shader_source}")
    endif()
    file(RELATIVE_PATH octaryn_client_shader_file
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_source}")
    list(APPEND octaryn_client_shader_bundle_outputs
        "${octaryn_client_bundle_dir}/Client/Shaders/${octaryn_client_shader_file}")
endforeach()

set(octaryn_client_shader_inventory "${client_build_root}/shaders/source-inventory.txt")
file(GENERATE OUTPUT "${octaryn_client_shader_inventory}"
    CONTENT "${octaryn_client_shader_sources}\n${octaryn_fsr2_vendor_sources}\n")

add_custom_command(
    OUTPUT "${octaryn_client_shader_stage_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders"
        "${octaryn_client_shader_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_FSR2_SHADER_VENDOR}" "${octaryn_client_shader_stage_dir}/Fsr2/Vendor"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_client_shader_stage_stamp}"
    DEPENDS "${octaryn_client_shader_inventory}"
        ${octaryn_client_shader_sources} ${octaryn_fsr2_vendor_sources}
    VERBATIM)

add_custom_target(octaryn_client_shaders
    DEPENDS "${octaryn_client_shader_stage_stamp}")
