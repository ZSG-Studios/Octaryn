include_guard(GLOBAL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/prepare_fsr2_shaders.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/acquire_fsr2.py")

# Upstream sources stay immutable and separate from first-party adapters.
set(OCTARYN_FSR2_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/fsr2-2.2.1-godot-2f698aa5"
    CACHE PATH "Pinned FSR 2.2.1 and Godot source cache")
set(OCTARYN_FSR2_SHADER_VENDOR "${OCTARYN_FSR2_ROOT}/slang")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${OCTARYN_FSR2_ROOT}/manifest.json")
if(NOT EXISTS "${OCTARYN_FSR2_ROOT}/manifest.json")
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/acquire_fsr2.py"
        --destination "${OCTARYN_FSR2_ROOT}" COMMAND_ERROR_IS_FATAL ANY)
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/prepare_fsr2_shaders.py"
    "${OCTARYN_FSR2_ROOT}" COMMAND_ERROR_IS_FATAL ANY)

add_library(octaryn_fsr2_sdk STATIC
    "${OCTARYN_FSR2_ROOT}/godot/ffx_fsr2.cpp"
    "${OCTARYN_FSR2_ROOT}/godot/ffx_assert.cpp")
target_include_directories(octaryn_fsr2_sdk SYSTEM PUBLIC "${OCTARYN_FSR2_ROOT}/godot")
target_compile_features(octaryn_fsr2_sdk PUBLIC cxx_std_17)
if(NOT WIN32)
    target_compile_definitions(octaryn_fsr2_sdk PUBLIC FFX_GCC)
endif()
set_target_properties(octaryn_fsr2_sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(octaryn_configure_fsr2 backend_target)
    octaryn_add_native_static_library(octaryn_client_fsr2 client
        SOURCES
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2/Fsr2.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2/Fsr2Resources.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2/Fsr2Pipelines.cpp"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2/Fsr2Jobs.cpp"
        PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/Fsr2"
        PRIVATE_LINKS octaryn_fsr2_sdk octaryn::deps::slang_rhi)
    target_include_directories(octaryn_client_fsr2 PRIVATE
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend")
    target_compile_definitions(octaryn_client_fsr2 PRIVATE
        OCTARYN_FSR2_SHADER_VENDOR="${OCTARYN_FSR2_SHADER_VENDOR}")
    target_link_libraries(${backend_target} PRIVATE octaryn_client_fsr2)
endfunction()
