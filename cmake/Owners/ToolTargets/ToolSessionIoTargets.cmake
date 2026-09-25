set(session_io_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession")
octaryn_add_native_static_library(octaryn_client_session_io_probe_files tools
    SOURCES "${session_io_source}/SessionFiles.cpp"
    PUBLIC_INCLUDE_DIRS "${session_io_source}")
target_compile_definitions(octaryn_client_session_io_probe_files PRIVATE
    read_text=probe_read_text write_text=probe_write_text)
octaryn_add_native_executable(octaryn_client_session_io_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientInteractionProbe/SessionIoProbe.cpp"
        "${session_io_source}/SessionIo.cpp"
    PRIVATE_LINKS octaryn_client_session_io_probe_files octaryn::deps::glaze)
set_target_properties(octaryn_client_session_io_probe octaryn_client_session_io_probe_files
    PROPERTIES EXCLUDE_FROM_ALL TRUE)
if(CMAKE_CROSSCOMPILING)
    add_custom_target(octaryn_validate_client_session_io
        COMMAND "${CMAKE_COMMAND}" -E echo "Cannot execute the session I/O probe while cross-compiling."
        COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)
else()
    add_custom_target(octaryn_validate_client_session_io
        COMMAND "${Python3_EXECUTABLE}" "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_session_io.py"
            "$<TARGET_FILE:octaryn_client_session_io_probe>"
            "${OCTARYN_BUILD_PRESET_ROOT}/client/validation/session-io"
        DEPENDS octaryn_client_session_io_probe
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)
endif()
unset(session_io_source)
