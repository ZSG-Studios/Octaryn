include_guard(GLOBAL)

include(Dependencies/DotNetHosting)
include(Dependencies/SourceDependencyCache)

find_package(Threads REQUIRED)

octaryn_add_dependency_wrapper(octaryn_native_threads octaryn::native_threads)
target_link_libraries(octaryn_native_threads INTERFACE Threads::Threads)

set(OCTARYN_NATIVE_SPDLOG_AVAILABLE OFF)
set(OCTARYN_NATIVE_CPPTRACE_AVAILABLE OFF)
set(OCTARYN_NATIVE_MIMALLOC_AVAILABLE OFF)
set(OCTARYN_NATIVE_TRACY_AVAILABLE OFF)
set(OCTARYN_NATIVE_JOLT_AVAILABLE OFF)

if(NOT TARGET octaryn::deps::glaze)
    octaryn_add_dependency_wrapper(octaryn_native_glaze octaryn::deps::glaze)
    octaryn_fetch_source_dependency(
        glaze
        GITHUB_REPOSITORY ${OCTARYN_DEP_glaze_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_glaze_TAG}
        OPTIONS ${OCTARYN_DEP_glaze_OPTIONS})
    if(TARGET glaze::glaze)
        target_link_libraries(octaryn_native_glaze INTERFACE glaze::glaze)
    endif()
endif()

octaryn_add_dependency_wrapper(octaryn_native_spdlog octaryn::deps::spdlog)
octaryn_fetch_source_dependency(
    spdlog
    GITHUB_REPOSITORY ${OCTARYN_DEP_spdlog_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_spdlog_TAG}
    OPTIONS ${OCTARYN_DEP_spdlog_OPTIONS})
if(TARGET spdlog::spdlog_header_only)
    target_link_libraries(octaryn_native_spdlog INTERFACE spdlog::spdlog_header_only)
    set(OCTARYN_NATIVE_SPDLOG_AVAILABLE ON)
elseif(TARGET spdlog::spdlog)
    target_link_libraries(octaryn_native_spdlog INTERFACE spdlog::spdlog)
    set(OCTARYN_NATIVE_SPDLOG_AVAILABLE ON)
else()
    message(STATUS "Target-compatible workspace spdlog target unavailable; octaryn_native_logging will use stdio fallback for this configure.")
endif()

octaryn_add_dependency_wrapper(octaryn_native_cpptrace octaryn::deps::cpptrace)
set(octaryn_cpptrace_options
    "CPPTRACE_BUILD_TESTING OFF"
    "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF OFF"
    "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE ON"
    "CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH ON"
    "BUILD_SHARED_LIBS OFF")
if(WIN32)
    list(APPEND octaryn_cpptrace_options
        # New Clang trips cpptrace's module detection, but CMake has no
        # dependency scanner for clang-cl: the explicit CXX_MODULES file set
        # fails generate. The engine uses no modules.
        "CPPTRACE_DISABLE_CXX_20_MODULES ON"
        "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE OFF"
        "CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH OFF"
        "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP ON")
endif()
octaryn_fetch_source_dependency(
    cpptrace
    GITHUB_REPOSITORY ${OCTARYN_DEP_cpptrace_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_cpptrace_TAG}
    OPTIONS
        ${octaryn_cpptrace_options})
if(TARGET cpptrace::cpptrace)
    target_link_libraries(octaryn_native_cpptrace INTERFACE cpptrace::cpptrace)
    set(OCTARYN_NATIVE_CPPTRACE_AVAILABLE ON)
endif()

octaryn_add_dependency_wrapper(octaryn_native_mimalloc octaryn::deps::mimalloc)
octaryn_fetch_source_dependency(
    mimalloc
    GITHUB_REPOSITORY ${OCTARYN_DEP_mimalloc_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_mimalloc_TAG}
    OPTIONS ${OCTARYN_DEP_mimalloc_OPTIONS})
if(TARGET mimalloc-static)
    target_link_libraries(octaryn_native_mimalloc INTERFACE mimalloc-static)
    set(OCTARYN_NATIVE_MIMALLOC_AVAILABLE ON)
elseif(TARGET mimalloc)
    target_link_libraries(octaryn_native_mimalloc INTERFACE mimalloc)
    set(OCTARYN_NATIVE_MIMALLOC_AVAILABLE ON)
endif()

octaryn_add_dependency_wrapper(octaryn_native_tracy octaryn::deps::tracy)
octaryn_fetch_source_dependency(
    tracy
    GITHUB_REPOSITORY ${OCTARYN_DEP_tracy_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_tracy_TAG}
    OPTIONS ${OCTARYN_DEP_tracy_OPTIONS})
if(TARGET Tracy::TracyClient)
    target_link_libraries(octaryn_native_tracy INTERFACE Tracy::TracyClient)
    set(OCTARYN_NATIVE_TRACY_AVAILABLE ON)
elseif(TARGET TracyClient)
    target_link_libraries(octaryn_native_tracy INTERFACE TracyClient)
    set(OCTARYN_NATIVE_TRACY_AVAILABLE ON)
endif()

octaryn_add_dependency_wrapper(octaryn_native_taskflow octaryn::deps::taskflow)
octaryn_fetch_header_dependency(
    Taskflow
    taskflow_source_dir
    GITHUB_REPOSITORY ${OCTARYN_DEP_taskflow_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_taskflow_TAG})
if(taskflow_source_dir)
    octaryn_add_header_only_dependency(octaryn_native_taskflow "${taskflow_source_dir}")
endif()

if(NOT TARGET octaryn::deps::eigen)
    octaryn_add_dependency_wrapper(octaryn_native_eigen octaryn::deps::eigen)
    octaryn_fetch_source_dependency(
        Eigen3
        GIT_REPOSITORY ${OCTARYN_DEP_eigen_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_eigen_TAG}
        OPTIONS ${OCTARYN_DEP_eigen_OPTIONS})
    octaryn_link_first_available_dependency(octaryn_native_eigen eigen_available Eigen3::Eigen)
endif()

if(NOT TARGET octaryn::deps::unordered_dense)
    octaryn_add_dependency_wrapper(octaryn_native_unordered_dense octaryn::deps::unordered_dense)
    octaryn_fetch_header_dependency(
        unordered_dense
        unordered_dense_source_dir
        GITHUB_REPOSITORY ${OCTARYN_DEP_unordered_dense_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_unordered_dense_TAG})
    if(EXISTS "${unordered_dense_source_dir}/include")
        octaryn_add_header_only_dependency(octaryn_native_unordered_dense "${unordered_dense_source_dir}/include")
    endif()
endif()

if(NOT TARGET octaryn::deps::zlib)
    octaryn_add_dependency_wrapper(octaryn_native_zlib octaryn::deps::zlib)
    set(octaryn_zlib_options ${OCTARYN_DEP_zlib_OPTIONS})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # The static archive links into shared owner libraries; it needs PIC.
        list(APPEND octaryn_zlib_options "CMAKE_POSITION_INDEPENDENT_CODE ON")
    endif()
    octaryn_fetch_source_dependency(
        zlib
        GITHUB_REPOSITORY ${OCTARYN_DEP_zlib_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_zlib_TAG}
        OPTIONS ${octaryn_zlib_options})
    octaryn_link_first_available_dependency(octaryn_native_zlib zlib_available
        ZLIB::ZLIBSTATIC zlibstatic ZLIB::ZLIB zlib)
endif()

if(NOT TARGET octaryn::deps::lz4)
    octaryn_add_dependency_wrapper(octaryn_native_lz4 octaryn::deps::lz4)
    octaryn_fetch_source_dependency(
        lz4
        GITHUB_REPOSITORY ${OCTARYN_DEP_lz4_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_lz4_TAG}
        SOURCE_SUBDIR ${OCTARYN_DEP_lz4_SOURCE_SUBDIR})
    octaryn_link_first_available_dependency(octaryn_native_lz4 lz4_available lz4::lz4 lz4_static)
endif()

if(NOT TARGET octaryn::deps::zstd)
    octaryn_add_dependency_wrapper(octaryn_native_zstd octaryn::deps::zstd)
    octaryn_fetch_source_dependency(
        zstd
        GITHUB_REPOSITORY ${OCTARYN_DEP_zstd_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_zstd_TAG}
        SOURCE_SUBDIR ${OCTARYN_DEP_zstd_SOURCE_SUBDIR}
        OPTIONS ${OCTARYN_DEP_zstd_OPTIONS})
    octaryn_link_first_available_dependency(octaryn_native_zstd zstd_available zstd::libzstd_static libzstd_static zstd::libzstd_shared libzstd_shared)
endif()

if(NOT TARGET octaryn::deps::jolt)
    octaryn_add_dependency_wrapper(octaryn_native_jolt octaryn::deps::jolt)
    set(octaryn_jolt_options ${OCTARYN_DEP_jolt_OPTIONS})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Jolt defaults to LTO bitcode objects, which GNU ld cannot consume.
        list(APPEND octaryn_jolt_options "INTERPROCEDURAL_OPTIMIZATION OFF")
    endif()
    octaryn_fetch_source_dependency(
        JoltPhysics
        GITHUB_REPOSITORY ${OCTARYN_DEP_jolt_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_jolt_TAG}
        SOURCE_SUBDIR ${OCTARYN_DEP_jolt_SOURCE_SUBDIR}
        OPTIONS ${octaryn_jolt_options})
    octaryn_link_first_available_dependency(octaryn_native_jolt jolt_available Jolt Jolt::Jolt)
    if(jolt_available)
        set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(OCTARYN_NATIVE_JOLT_AVAILABLE ON)
    endif()
endif()
