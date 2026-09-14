octaryn_add_native_executable(octaryn_client_fsr2_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientFsr2Probe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientFsr2Probe/TemporalInputs.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientFsr2Probe/JitterRaster.cpp"
    PRIVATE_LINKS octaryn_client_render_backend octaryn_client_fsr2 octaryn::deps::slang_rhi)
set_target_properties(octaryn_client_fsr2_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(fsr2_fixture "${OCTARYN_BUILD_PRESET_ROOT}/tools/validation/fsr2")
set(fsr2_runtime_commands)
foreach(runtime_file IN LISTS OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
    list(APPEND fsr2_runtime_commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${runtime_file}" "${fsr2_fixture}/")
endforeach()
add_custom_target(octaryn_stage_client_fsr2_probe
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${fsr2_fixture}/Client/Shaders/Fsr2"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:octaryn_client_fsr2_probe>" "${fsr2_fixture}/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/Fsr2"
        "${fsr2_fixture}/Client/Shaders/Fsr2"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OCTARYN_FSR2_SHADER_VENDOR}"
        "${fsr2_fixture}/Client/Shaders/Fsr2/Vendor"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/Temporal"
        "${fsr2_fixture}/Client/Shaders/Temporal"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${fsr2_fixture}/Client/Shaders/Sky"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/Sky/SkyRay.slang"
        "${fsr2_fixture}/Client/Shaders/Sky/SkyRay.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientFsr2Probe/JitterRaster.slang"
        "${fsr2_fixture}/Client/Shaders/Sky/JitterRaster.slang"
    ${fsr2_runtime_commands}
    DEPENDS octaryn_client_fsr2_probe
    VERBATIM)
foreach(backend IN ITEMS vulkan dx12 metal)
    add_custom_target(octaryn_validate_client_fsr2_${backend}
        COMMAND "${fsr2_fixture}/$<TARGET_FILE_NAME:octaryn_client_fsr2_probe>" "${backend}"
        DEPENDS octaryn_stage_client_fsr2_probe
        WORKING_DIRECTORY "${fsr2_fixture}"
        VERBATIM)
endforeach()
