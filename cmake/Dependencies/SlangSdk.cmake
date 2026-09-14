include_guard(GLOBAL)

if(NOT WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/SlangRhiUnix.cmake")
    return()
endif()

set(OCTARYN_SLANG_SDK_ROOT "$ENV{SLANG_SDK_ROOT}" CACHE PATH "Slang compiler SDK root")
if(NOT OCTARYN_SLANG_SDK_ROOT)
    set(OCTARYN_SLANG_SDK_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/slang-2026.17.1" CACHE PATH "Slang compiler SDK root" FORCE)
endif()
set(OCTARYN_SLANG_RHI_SOURCE_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/slang-rhi" CACHE PATH "Pinned standalone slang-rhi checkout")
set(rhi_configuration "Release")
if(CMAKE_BUILD_TYPE)
    set(rhi_configuration "${CMAKE_BUILD_TYPE}")
endif()
set(OCTARYN_SLANG_RHI_BUILD_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/slang-rhi-windows-${OCTARYN_TARGET_ARCH}-${rhi_configuration}" CACHE PATH "Standalone static DX12/Vulkan slang-rhi build")
set(OCTARYN_SLANG_RHI_COMMIT "e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc")

# Clear the former GFX discovery results: these names previously pointed at gfx.lib.
unset(OCTARYN_SLANG_RHI_INCLUDE_DIR CACHE)
unset(OCTARYN_SLANG_RHI_LIBRARY CACHE)
unset(OCTARYN_SLANGC_EXECUTABLE CACHE)
set(OCTARYN_SLANG_RHI_INCLUDE_DIR "${OCTARYN_SLANG_RHI_SOURCE_ROOT}/include")
find_program(OCTARYN_SLANGC_EXECUTABLE NAMES slangc PATHS "${OCTARYN_SLANG_SDK_ROOT}/bin" NO_DEFAULT_PATH)
foreach(required_file IN ITEMS
    "${OCTARYN_SLANG_RHI_INCLUDE_DIR}/slang-rhi.h"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/include/slang-rhi-config.h"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/octaryn-dependency.cmake"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi.lib"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-d3d12ma.lib"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-vma.lib"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-resources.lib"
    "${OCTARYN_SLANG_SDK_ROOT}/include/slang.h"
    "${OCTARYN_SLANG_SDK_ROOT}/lib/slang-compiler.lib"
    "${OCTARYN_SLANGC_EXECUTABLE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Standalone slang-rhi prerequisite missing: ${required_file}. Run tools/build/slang-rhi.ps1, then configure again.")
    endif()
endforeach()
include("${OCTARYN_SLANG_RHI_BUILD_ROOT}/octaryn-dependency.cmake")
file(REAL_PATH "${OCTARYN_SLANG_SDK_ROOT}" sdk_root)
file(REAL_PATH "${OCTARYN_SLANG_RHI_BUILT_SDK_ROOT}" built_sdk_root)
if(NOT OCTARYN_SLANG_RHI_BUILT_COMMIT STREQUAL OCTARYN_SLANG_RHI_COMMIT OR NOT sdk_root STREQUAL built_sdk_root)
    message(FATAL_ERROR "Standalone slang-rhi build uses a different source pin or Slang SDK. Re-run tools/build/slang-rhi.ps1 with the selected SDK.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT OCTARYN_SLANG_RHI_BUILT_ARCH STREQUAL OCTARYN_TARGET_ARCH)
    message(FATAL_ERROR "Standalone slang-rhi build architecture differs from the client target. Build the matching dependency architecture.")
endif()
if((CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT OCTARYN_SLANG_RHI_BUILT_CONFIG STREQUAL "Debug") OR
   (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND OCTARYN_SLANG_RHI_BUILT_CONFIG STREQUAL "Debug"))
    message(FATAL_ERROR "Client and standalone slang-rhi CRT configurations differ. Run tools/build/slang-rhi.ps1 -Configuration ${rhi_configuration} and select its build directory.")
endif()

octaryn_add_dependency_wrapper(octaryn_client_slang_rhi octaryn::deps::slang_rhi)
add_library(octaryn_slang_rhi_native STATIC IMPORTED GLOBAL)
set_target_properties(octaryn_slang_rhi_native PROPERTIES
    IMPORTED_LOCATION "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi.lib"
    INTERFACE_LINK_LIBRARIES "${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-d3d12ma.lib;d3d12;dxgi;dxguid;${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-vma.lib;${OCTARYN_SLANG_RHI_BUILD_ROOT}/slang-rhi-resources.lib;${OCTARYN_SLANG_SDK_ROOT}/lib/slang-compiler.lib")
target_include_directories(octaryn_client_slang_rhi SYSTEM INTERFACE
    "${OCTARYN_SLANG_RHI_INCLUDE_DIR}" "${OCTARYN_SLANG_RHI_BUILD_ROOT}/include" "${OCTARYN_SLANG_SDK_ROOT}/include")
target_compile_definitions(octaryn_client_slang_rhi INTERFACE SLANG_USER_CONFIG="slang-user-config.h")
target_link_libraries(octaryn_client_slang_rhi INTERFACE octaryn_slang_rhi_native)
set(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE ON)
set(OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
# slang-glslang supplies Slang's SPIR-V optimizer even for direct Slang emission.
foreach(runtime_name IN ITEMS slang slang-compiler slang-rt slang-glslang)
    set(runtime_file "${OCTARYN_SLANG_SDK_ROOT}/bin/${runtime_name}.dll")
    if(NOT EXISTS "${runtime_file}")
        message(FATAL_ERROR "Slang compiler runtime is incomplete: ${runtime_file}")
    endif()
    list(APPEND OCTARYN_CLIENT_SLANG_RUNTIME_FILES "${runtime_file}")
endforeach()
message(STATUS "Client renderer: standalone slang-rhi ${OCTARYN_SLANG_RHI_COMMIT}, static DX12/Vulkan, Slang SDK ${OCTARYN_SLANG_SDK_ROOT}")

foreach(runtime_name IN ITEMS dxcompiler dxil)
    set(runtime_file "${OCTARYN_SLANG_RHI_BUILD_ROOT}/${runtime_name}.dll")
    if(NOT EXISTS "${runtime_file}")
        message(FATAL_ERROR "DX12 compiler runtime missing: ${runtime_file}. Run tools/build/slang-rhi.ps1.")
    endif()
    list(APPEND OCTARYN_CLIENT_SLANG_RUNTIME_FILES "${runtime_file}")
endforeach()
