include_guard(GLOBAL)

set(OCTARYN_DOTNET_ROOT "$ENV{DOTNET_ROOT}" CACHE PATH "Root of the .NET installation used for native hosting")
if(NOT OCTARYN_DOTNET_ROOT)
    file(REAL_PATH "${DOTNET_EXECUTABLE}" octaryn_dotnet_executable_path)
    get_filename_component(octaryn_dotnet_install_dir "${octaryn_dotnet_executable_path}" DIRECTORY)
    set(OCTARYN_DOTNET_ROOT "${octaryn_dotnet_install_dir}" CACHE PATH "Root of the .NET installation used for native hosting" FORCE)
endif()

if(NOT OCTARYN_TARGET_DOTNET_RID)
    message(FATAL_ERROR "A target .NET RID is required for native hosting.")
endif()

execute_process(COMMAND "${DOTNET_EXECUTABLE}" --list-sdks
    OUTPUT_VARIABLE octaryn_dotnet_sdks RESULT_VARIABLE octaryn_dotnet_sdk_result)
if(NOT octaryn_dotnet_sdk_result EQUAL 0 OR NOT octaryn_dotnet_sdks MATCHES "(^|\n)10\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "Octaryn requires an installed .NET 10 SDK.")
endif()

set(octaryn_dotnet_host_rids "${OCTARYN_TARGET_DOTNET_RID}")
if(NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env DOTNET_CLI_UI_LANGUAGE=en
        "${DOTNET_EXECUTABLE}" --info OUTPUT_VARIABLE octaryn_dotnet_info)
    if(octaryn_dotnet_info MATCHES "RID:[ \t]+([^ \r\n]+)")
        list(APPEND octaryn_dotnet_host_rids "${CMAKE_MATCH_1}")
    endif()
endif()
list(REMOVE_DUPLICATES octaryn_dotnet_host_rids)
set(octaryn_dotnet_host_pack_patterns)
foreach(rid IN LISTS octaryn_dotnet_host_rids)
    set(host_pack "Microsoft.NETCore.App.Host.${rid}")
    string(TOLOWER "${host_pack}" host_pack_cache)
    list(APPEND octaryn_dotnet_host_pack_patterns
        "${OCTARYN_DOTNET_ROOT}/packs/${host_pack}/10.*/runtimes/${rid}/native"
        "${OCTARYN_NUGET_PACKAGES_DIR}/${host_pack_cache}/10.*/runtimes/${rid}/native")
endforeach()
file(GLOB OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS LIST_DIRECTORIES true ${octaryn_dotnet_host_pack_patterns})

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

file(GLOB OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS LIST_DIRECTORIES true ${octaryn_dotnet_host_pack_patterns})
list(SORT OCTARYN_DOTNET_HOST_PACK_NATIVE_DIRS COMPARE NATURAL ORDER DESCENDING)

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
    find_file(OCTARYN_DOTNET_NETHOST_RUNTIME
        NAMES libnethost.so libnethost.dylib
        PATHS "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
endif()

foreach(header IN ITEMS nethost.h hostfxr.h coreclr_delegates.h)
    if(NOT EXISTS "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}/${header}")
        message(FATAL_ERROR "Missing .NET 10 native hosting header ${header}; install the target SDK host pack.")
    endif()
endforeach()
if(NOT OCTARYN_DOTNET_NETHOST_RUNTIME OR
   (OCTARYN_TARGET_PLATFORM STREQUAL "Windows" AND NOT OCTARYN_DOTNET_NETHOST_LIBRARY))
    message(FATAL_ERROR "Missing .NET 10 nethost library/runtime for ${OCTARYN_TARGET_DOTNET_RID}; set OCTARYN_DOTNET_ROOT to the SDK installation.")
endif()
set(OCTARYN_DOTNET_HOSTING_AVAILABLE ON)
get_filename_component(OCTARYN_DOTNET_NETHOST_RUNTIME_NAME "${OCTARYN_DOTNET_NETHOST_RUNTIME}" NAME)

add_library(octaryn_dotnet_hosting INTERFACE)
add_library(octaryn::dotnet_hosting ALIAS octaryn_dotnet_hosting)

target_include_directories(octaryn_dotnet_hosting
    INTERFACE
        "${OCTARYN_DOTNET_HOSTING_INCLUDE_DIR}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")

add_library(octaryn_dotnet_nethost SHARED IMPORTED GLOBAL)
set_target_properties(octaryn_dotnet_nethost PROPERTIES
    IMPORTED_LOCATION "${OCTARYN_DOTNET_NETHOST_RUNTIME}")
if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
    set_target_properties(octaryn_dotnet_nethost PROPERTIES
        IMPORTED_IMPLIB "${OCTARYN_DOTNET_NETHOST_LIBRARY}")
else()
    set(OCTARYN_DOTNET_NETHOST_LIBRARY "${OCTARYN_DOTNET_NETHOST_RUNTIME}")
endif()
target_link_libraries(octaryn_dotnet_hosting INTERFACE octaryn_dotnet_nethost ${CMAKE_DL_LIBS})

function(octaryn_stage_dotnet_host_runtime target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_DOTNET_NETHOST_RUNTIME}" "$<TARGET_FILE_DIR:${target_name}>"
        VERBATIM)
    if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux")
        set_target_properties(${target_name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH FALSE)
        set_property(TARGET ${target_name} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endfunction()
