include_guard(GLOBAL)

include(FetchContent)

if(POLICY CMP0097)
    cmake_policy(SET CMP0097 NEW)
endif()

option(OCTARYN_ENABLE_SOURCE_DEPENDENCIES "Fetch approved third-party sources into build/dependencies and build them under build/<preset>/deps." ON)
option(OCTARYN_USE_GITHUB_ARCHIVES_FOR_SOURCE_DEPENDENCIES "Download GitHub source dependencies as archives when submodules are not required." ON)
option(OCTARYN_USE_GITHUB_ARCHIVES_FOR_HEADER_DEPENDENCIES "Download GitHub header-only dependencies as archives instead of cloning full repositories." ON)

set(OCTARYN_SOURCE_DEPENDENCY_ROOT
    "${OCTARYN_DEPENDENCY_ROOT}"
    CACHE PATH
    "Shared root for fetched third-party dependency sources and downloads.")

set(OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT
    "${OCTARYN_DEPENDENCY_SOURCE_ROOT}"
    CACHE PATH
    "Shared root for fetched third-party dependency source trees.")
set(OCTARYN_SOURCE_DEPENDENCY_DOWNLOAD_ROOT
    "${OCTARYN_DEPENDENCY_DOWNLOAD_ROOT}"
    CACHE PATH
    "Shared root for downloaded third-party dependency archives.")
set(OCTARYN_SOURCE_DEPENDENCY_BUILD_ROOT
    "${OCTARYN_DEPENDENCY_BUILD_ROOT}/build"
    CACHE PATH
    "Preset-scoped root for third-party dependency build trees.")
set(OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT
    "${OCTARYN_DEPENDENCY_BUILD_ROOT}/stamps"
    CACHE PATH
    "Preset-scoped root for third-party dependency population stamps.")

set(FETCHCONTENT_BASE_DIR "${OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT}" CACHE PATH "Octaryn FetchContent cache root." FORCE)
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent dependency progress." FORCE)
set(CPM_SOURCE_CACHE "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}" CACHE PATH "Shared source cache for dependencies that use CPM internally." FORCE)
set(CPM_MODULE_PATH "${OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT}/cpm-modules" CACHE PATH "Preset-scoped CPM module path." FORCE)
set(CPM_DONT_CREATE_PACKAGE_LOCK ON CACHE BOOL "Do not generate preset-local CPM lock files in build output." FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Workspace dependency builds do not generate install rules." FORCE)

function(octaryn_dependency_github_url output_var repository)
    set(${output_var} "https://github.com/${repository}.git" PARENT_SCOPE)
endfunction()

function(octaryn_dependency_github_archive_url output_var repository reference)
    set(${output_var} "https://github.com/${repository}/archive/${reference}.tar.gz" PARENT_SCOPE)
endfunction()

function(octaryn_use_existing_dependency_source dependency_name dependency_key)
    set(dependency_source_dir "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}")
    if(EXISTS "${dependency_source_dir}")
        file(GLOB dependency_source_entries LIST_DIRECTORIES true "${dependency_source_dir}/*")
        if(dependency_source_entries)
            string(TOUPPER "${dependency_name}" dependency_name_upper)
            set("FETCHCONTENT_SOURCE_DIR_${dependency_name_upper}" "${dependency_source_dir}" CACHE PATH "Existing source for ${dependency_name}." FORCE)
        endif()
    endif()
endfunction()

function(octaryn_add_dependency_wrapper wrapper_target alias_target)
    if(NOT TARGET ${wrapper_target})
        add_library(${wrapper_target} INTERFACE)
    endif()

    if(NOT TARGET ${alias_target})
        add_library(${alias_target} ALIAS ${wrapper_target})
    endif()
endfunction()

function(octaryn_link_first_available_dependency wrapper_target output_var)
    set(found_dependency OFF)
    foreach(dependency_target IN LISTS ARGN)
        if(TARGET ${dependency_target})
            target_link_libraries(${wrapper_target} INTERFACE ${dependency_target})
            set(found_dependency ON)
            break()
        endif()
    endforeach()

    set(${output_var} "${found_dependency}" PARENT_SCOPE)
endfunction()

function(octaryn_fetch_source_dependency dependency_name)
    if(NOT OCTARYN_ENABLE_SOURCE_DEPENDENCIES)
        return()
    endif()

    cmake_parse_arguments(
        OCTARYN_FETCH
        "FORCE_GIT;NO_GIT_SUBMODULES"
        "GITHUB_REPOSITORY;GIT_REPOSITORY;GIT_TAG;URL;URL_HASH;VERSION;SOURCE_SUBDIR;GIT_SUBMODULES"
        "OPTIONS"
        ${ARGN})

    foreach(option_assignment IN LISTS OCTARYN_FETCH_OPTIONS)
        string(REPLACE " " ";" option_parts "${option_assignment}")
        list(LENGTH option_parts option_part_count)
        if(option_part_count GREATER 1)
            list(GET option_parts 0 option_name)
            list(REMOVE_AT option_parts 0)
            string(REPLACE ";" " " option_value "${option_parts}")
            set(${option_name} "${option_value}" CACHE STRING "Option for ${dependency_name}" FORCE)
        endif()
    endforeach()

    string(TOLOWER "${dependency_name}" dependency_key)
    octaryn_use_existing_dependency_source("${dependency_name}" "${dependency_key}")

    if(OCTARYN_FETCH_URL)
        set(declare_args
            URL "${OCTARYN_FETCH_URL}"
            DOWNLOAD_DIR "${OCTARYN_SOURCE_DEPENDENCY_DOWNLOAD_ROOT}/${dependency_key}")
        if(OCTARYN_FETCH_URL_HASH)
            list(APPEND declare_args URL_HASH "${OCTARYN_FETCH_URL_HASH}")
        endif()
    else()
        if(OCTARYN_FETCH_GIT_REPOSITORY)
            set(dependency_repository "${OCTARYN_FETCH_GIT_REPOSITORY}")
            set(dependency_github_repository "")
        elseif(OCTARYN_FETCH_GITHUB_REPOSITORY)
            octaryn_dependency_github_url(dependency_repository "${OCTARYN_FETCH_GITHUB_REPOSITORY}")
            set(dependency_github_repository "${OCTARYN_FETCH_GITHUB_REPOSITORY}")
        else()
            message(FATAL_ERROR "Source dependency ${dependency_name} must declare URL, GIT_REPOSITORY, or GITHUB_REPOSITORY.")
        endif()

        if(OCTARYN_FETCH_GIT_TAG)
            set(dependency_tag "${OCTARYN_FETCH_GIT_TAG}")
        elseif(OCTARYN_FETCH_VERSION)
            set(dependency_tag "${OCTARYN_FETCH_VERSION}")
        else()
            message(FATAL_ERROR "Source dependency ${dependency_name} must declare GIT_TAG or VERSION.")
        endif()

        set(dependency_needs_git OFF)
        if(OCTARYN_FETCH_FORCE_GIT OR OCTARYN_FETCH_GIT_SUBMODULES)
            set(dependency_needs_git ON)
        endif()

        if(OCTARYN_USE_GITHUB_ARCHIVES_FOR_SOURCE_DEPENDENCIES AND dependency_github_repository AND NOT dependency_needs_git)
            octaryn_dependency_github_archive_url(dependency_archive_url "${dependency_github_repository}" "${dependency_tag}")
            set(declare_args
                URL "${dependency_archive_url}"
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                DOWNLOAD_DIR "${OCTARYN_SOURCE_DEPENDENCY_DOWNLOAD_ROOT}/${dependency_key}")
        else()
            set(declare_args
                GIT_REPOSITORY "${dependency_repository}"
                GIT_TAG "${dependency_tag}"
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE)
        endif()
    endif()

    if(NOT OCTARYN_FETCH_NO_GIT_SUBMODULES AND DEFINED OCTARYN_FETCH_GIT_SUBMODULES)
        list(APPEND declare_args GIT_SUBMODULES "${OCTARYN_FETCH_GIT_SUBMODULES}")
    endif()

    if(OCTARYN_FETCH_SOURCE_SUBDIR)
        list(APPEND declare_args SOURCE_SUBDIR "${OCTARYN_FETCH_SOURCE_SUBDIR}")
    endif()

    list(APPEND declare_args
        SOURCE_DIR "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}"
        BINARY_DIR "${OCTARYN_SOURCE_DEPENDENCY_BUILD_ROOT}/${dependency_key}"
        SUBBUILD_DIR "${OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT}/${dependency_key}")

    if(OCTARYN_FETCH_NO_GIT_SUBMODULES)
        # Keep the empty argument quoted; list expansion otherwise drops it.
        FetchContent_Declare(${dependency_name} ${declare_args} GIT_SUBMODULES "")
    else()
        FetchContent_Declare(${dependency_name} ${declare_args})
    endif()
    FetchContent_MakeAvailable(${dependency_name})
endfunction()

function(octaryn_fetch_header_dependency dependency_name output_var)
    if(NOT OCTARYN_ENABLE_SOURCE_DEPENDENCIES)
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    cmake_parse_arguments(
        OCTARYN_FETCH
        ""
        "GITHUB_REPOSITORY;GIT_REPOSITORY;GIT_TAG;VERSION"
        ""
        ${ARGN})

    if(OCTARYN_FETCH_GIT_REPOSITORY)
        set(dependency_repository "${OCTARYN_FETCH_GIT_REPOSITORY}")
        set(dependency_github_repository "")
    elseif(OCTARYN_FETCH_GITHUB_REPOSITORY)
        octaryn_dependency_github_url(dependency_repository "${OCTARYN_FETCH_GITHUB_REPOSITORY}")
        set(dependency_github_repository "${OCTARYN_FETCH_GITHUB_REPOSITORY}")
    else()
        message(FATAL_ERROR "Header dependency ${dependency_name} must declare GIT_REPOSITORY or GITHUB_REPOSITORY.")
    endif()

    if(OCTARYN_FETCH_GIT_TAG)
        set(dependency_tag "${OCTARYN_FETCH_GIT_TAG}")
    elseif(OCTARYN_FETCH_VERSION)
        set(dependency_tag "${OCTARYN_FETCH_VERSION}")
    else()
        message(FATAL_ERROR "Header dependency ${dependency_name} must declare GIT_TAG or VERSION.")
    endif()

    string(TOLOWER "${dependency_name}" dependency_key)
    octaryn_use_existing_dependency_source("${dependency_name}" "${dependency_key}")

    if(OCTARYN_USE_GITHUB_ARCHIVES_FOR_HEADER_DEPENDENCIES AND dependency_github_repository)
        octaryn_dependency_github_archive_url(dependency_archive_url "${dependency_github_repository}" "${dependency_tag}")
        set(populate_args
            URL "${dependency_archive_url}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            DOWNLOAD_DIR "${OCTARYN_SOURCE_DEPENDENCY_DOWNLOAD_ROOT}/${dependency_key}"
            SOURCE_DIR "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}"
            BINARY_DIR "${OCTARYN_SOURCE_DEPENDENCY_BUILD_ROOT}/${dependency_key}"
            SUBBUILD_DIR "${OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT}/${dependency_key}")
    else()
        set(populate_args
            GIT_REPOSITORY "${dependency_repository}"
            GIT_TAG "${dependency_tag}"
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
            SOURCE_DIR "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}"
            BINARY_DIR "${OCTARYN_SOURCE_DEPENDENCY_BUILD_ROOT}/${dependency_key}"
            SUBBUILD_DIR "${OCTARYN_SOURCE_DEPENDENCY_STAMP_ROOT}/${dependency_key}")
    endif()

    file(GLOB dependency_source_entries LIST_DIRECTORIES true "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}/*")
    if(NOT dependency_source_entries)
        FetchContent_Populate(${dependency_name} ${populate_args})
    endif()

    set(${output_var} "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/${dependency_key}" PARENT_SCOPE)
endfunction()

function(octaryn_add_header_only_dependency wrapper_target include_dir)
    if(NOT TARGET ${wrapper_target})
        add_library(${wrapper_target} INTERFACE)
    endif()

    if(include_dir)
        target_include_directories(${wrapper_target}
            SYSTEM INTERFACE
                "${include_dir}")
    endif()
endfunction()
