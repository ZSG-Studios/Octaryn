octaryn_add_native_executable(octaryn_client_resource_retirement_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientResourceRetirementProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientResourceRetirementProbe/Worker.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientResourceRetirementProbe/Descriptors.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientResourceRetirementProbe/Progress.cpp"
    PRIVATE_LINKS octaryn::deps::slang_rhi)
target_include_directories(octaryn_client_resource_retirement_probe SYSTEM PRIVATE
    "${OCTARYN_SLANG_RHI_SOURCE_ROOT}/src"
    "${OCTARYN_SLANG_RHI_SOURCE_ROOT}/external/d3d12ma")
target_compile_definitions(octaryn_client_resource_retirement_probe PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN SLANG_RHI_DEBUG=0)
set_target_properties(octaryn_client_resource_retirement_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_client_resource_retirement
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientResourceRetirementProbe/run.py"
        "$<TARGET_FILE:octaryn_client_resource_retirement_probe>"
    DEPENDS octaryn_client_resource_retirement_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
