include_guard(GLOBAL)

set(OCTARYN_DOTNET_ROOT "$ENV{DOTNET_ROOT}" CACHE PATH "Root of the .NET installation used for native hosting")
if(NOT OCTARYN_DOTNET_ROOT)
    file(REAL_PATH "${DOTNET_EXECUTABLE}" octaryn_dotnet_executable_path)
    get_filename_component(octaryn_dotnet_install_dir "${octaryn_dotnet_executable_path}" DIRECTORY)
    set(OCTARYN_DOTNET_ROOT "${octaryn_dotnet_install_dir}" CACHE PATH "Root of the .NET installation used for native hosting" FORCE)
endif()

if(NOT OCTARYN_TARGET_DOTNET_RID)
    message(STATUS
        "Octaryn .NET target RID is not declared for ${OCTARYN_TARGET_PLATFORM}; "
        "hostfxr bridge targets are disabled for this configure.")
    set(OCTARYN_DOTNET_HOSTING_AVAILABLE OFF)
    return()
endif()

set(octaryn_dotnet_host_pack_name "Microsoft.NETCore.App.Host.${OCTARYN_TARGET_DOTNET_RID}")
string(TOLOWER "${octaryn_dotnet_host_pack_name}" octaryn_dotnet_host_pack_cache_name)

file(GLOB OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS
    LIST_DIRECTORIES true
    "${OCTARYN_DOTNET_ROOT}/packs/${octaryn_dotnet_host_pack_name}/[0-9]*/runtimes/${OCTARYN_TARGET_DOTNET_RID}/native"
    "${OCTARYN_NUGET_PACKAGES_DIR}/${octaryn_dotnet_host_pack_cache_name}/[0-9]*/runtimes/${OCTARYN_TARGET_DOTNET_RID}/native")

unset(OCTARYN_DOTNET_HOSTING_INCLUDE_DIR CACHE)
unset(OCTARYN_DOTNET_NETHOST_LIBRARY CACHE)
unset(OCTARYN_DOTNET_NETHOST_RUNTIME CACHE)

if(NOT OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS)
    execute_process(
        COMMAND "${DOTNET_EXECUTABLE}" restore
            "${OCTARYN_WORKSPACE_ROOT_DIR}/Octaryn.DotNet.sln"
            --runtime "${OCTARYN_TARGET_DOTNET_RID}"
            --packages "${OCTARYN_NUGET_PACKAGES_DIR}"
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        RESULT_VARIABLE octaryn_dotnet_host_pack_restore_result
        OUTPUT_QUIET
        ERROR_VARIABLE octaryn_dotnet_host_pack_restore_error)
    if(NOT octaryn_dotnet_host_pack_restore_result EQUAL 0)
        message(STATUS
            "Octaryn .NET target host pack restore failed for ${OCTARYN_TARGET_DOTNET_RID}: "
            "${octaryn_dotnet_host_pack_restore_error}")
    endif()
endif()

file(GLOB OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS
    LIST_DIRECTORIES true
    "${OCTARYN_DOTNET_ROOT}/packs/${octaryn_dotnet_host_pack_name}/[0-9]*/runtimes/${OCTARYN_TARGET_DOTNET_RID}/native"
    "${OCTARYN_NUGET_PACKAGES_DIR}/${octaryn_dotnet_host_pack_cache_name}/[0-9]*/runtimes/${OCTARYN_TARGET_DOTNET_RID}/native")

list(SORT OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS COMPARE NATURAL ORDER DESCENDING)

if(NOT OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
    set(OCTARYN_DOTNET_HOSTFXR_LIBRARY_GLOB "${OCTARYN_DOTNET_ROOT}/host/fxr/*/libhostfxr.so")
endif()

find_path(OCTARYN_DOTNET_HOSTING_INCLUDE_DIR
    NAMES nethost.h hostfxr.h coreclr_delegates.h
    PATHS ${OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH)

if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
    find_file(OCTARYN_DOTNET_NETHOST_LIBRARY
        NAMES nethost.lib
        PATHS "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_file(OCTARYN_DOTNET_NETHOST_RUNTIME
        NAMES nethost.dll
        PATHS "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
else()
    find_library(OCTARYN_DOTNET_NETHOST_LIBRARY
        NAMES nethost libnethost
        PATHS "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
endif()

if(OCTARYN_DOTNET_HOSTFXR_LIBRARY_GLOB)
    file(GLOB OCTARYN_DOTNET_HOSTFXR_LIBRARIES
        "${OCTARYN_DOTNET_HOSTFXR_LIBRARY_GLOB}")
    list(SORT OCTARYN_DOTNET_HOSTFXR_LIBRARIES COMPARE NATURAL ORDER DESCENDING)
    if(OCTARYN_DOTNET_HOSTFXR_LIBRARIES)
        list(GET OCTARYN_DOTNET_HOSTFXR_LIBRARIES 0 OCTARYN_DOTNET_HOSTFXR_LIBRARY)
    endif()
endif()

set(OCTARYN_DOTNET_HOSTING_AVAILABLE OFF)
if(OCTARYN_DOTNET_HOSTING_INCLUDE_DIR AND OCTARYN_DOTNET_NETHOST_LIBRARY)
    if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows" AND OCTARYN_DOTNET_NETHOST_RUNTIME)
        set(OCTARYN_DOTNET_HOSTING_AVAILABLE ON)
    elseif(NOT OCTARYN_TARGET_PLATFORM STREQUAL "Windows" AND OCTARYN_DOTNET_HOSTFXR_LIBRARY)
        set(OCTARYN_DOTNET_HOSTING_AVAILABLE ON)
    endif()
endif()

if(NOT OCTARYN_DOTNET_HOSTING_AVAILABLE)
    if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
        message(FATAL_ERROR
            "Windows .NET hosting requires hostfxr.h, nethost.lib and nethost.dll for ${OCTARYN_TARGET_DOTNET_RID}. "
            "Install the .NET SDK or set OCTARYN_DOTNET_ROOT to its installation directory.")
    endif()
    message(STATUS
        "Octaryn .NET native hosting assets unavailable for ${OCTARYN_TARGET_PLATFORM}/${OCTARYN_TARGET_DOTNET_RID} under ${OCTARYN_DOTNET_ROOT}; "
        "hostfxr bridge targets are disabled for this configure.")
    return()
endif()

add_library(octaryn_dotnet_hosting INTERFACE)
add_library(octaryn::dotnet_hosting ALIAS octaryn_dotnet_hosting)

target_include_directories(octaryn_dotnet_hosting
    INTERFACE
        "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")

if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
    add_library(octaryn_dotnet_nethost SHARED IMPORTED GLOBAL)
    set_target_properties(octaryn_dotnet_nethost PROPERTIES
        IMPORTED_IMPLIB "${OCTARYN_DOTNET_NETHOST_LIBRARY}"
        IMPORTED_LOCATION "${OCTARYN_DOTNET_NETHOST_RUNTIME}")
    target_link_libraries(octaryn_dotnet_hosting INTERFACE octaryn_dotnet_nethost)
else()
    target_link_libraries(octaryn_dotnet_hosting
        INTERFACE
            "${OCTARYN_DOTNET_NETHOST_LIBRARY}")
endif()

if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux")
    target_link_libraries(octaryn_dotnet_hosting INTERFACE dl)
endif()

if(NOT OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
    target_compile_definitions(octaryn_dotnet_hosting
        INTERFACE
            OCTARYN_DOTNET_HOSTFXR_PATH="${OCTARYN_DOTNET_HOSTFXR_LIBRARY}")
endif()

function(octaryn_stage_dotnet_host_runtime target_name)
    if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${OCTARYN_DOTNET_NETHOST_RUNTIME}" "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM)
    endif()
endfunction()
