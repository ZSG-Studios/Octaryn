include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

find_package(Threads REQUIRED)

octaryn_add_dependency_wrapper(octaryn_native_threads octaryn::native_threads)
target_link_libraries(octaryn_native_threads INTERFACE Threads::Threads)

set(OCTARYN_NATIVE_SPDLOG_AVAILABLE OFF)
set(OCTARYN_NATIVE_MIMALLOC_AVAILABLE OFF)
set(OCTARYN_NATIVE_TRACY_AVAILABLE OFF)
set(OCTARYN_NATIVE_BOX3D_AVAILABLE OFF)

if(NOT TARGET octaryn::deps::glaze)
    octaryn_add_dependency_wrapper(octaryn_native_glaze octaryn::deps::glaze)
    octaryn_fetch_source_dependency(
        glaze
        GITHUB_REPOSITORY stephenberry/glaze
        GIT_TAG v7.4.0
        OPTIONS
            "glaze_BUILD_TESTS OFF")
    if(TARGET glaze::glaze)
        target_link_libraries(octaryn_native_glaze INTERFACE glaze::glaze)
    endif()
endif()

octaryn_add_dependency_wrapper(octaryn_native_spdlog octaryn::deps::spdlog)
octaryn_fetch_source_dependency(
    spdlog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG v1.17.0
    OPTIONS
        "SPDLOG_BUILD_SHARED OFF"
        "SPDLOG_BUILD_TESTS OFF"
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_BUILD_BENCH OFF"
        "SPDLOG_FMT_EXTERNAL OFF"
        "SPDLOG_USE_STD_FORMAT OFF")
if(TARGET spdlog::spdlog_header_only)
    target_link_libraries(octaryn_native_spdlog INTERFACE spdlog::spdlog_header_only)
    set(OCTARYN_NATIVE_SPDLOG_AVAILABLE ON)
elseif(TARGET spdlog::spdlog)
    target_link_libraries(octaryn_native_spdlog INTERFACE spdlog::spdlog)
    set(OCTARYN_NATIVE_SPDLOG_AVAILABLE ON)
else()
    message(STATUS "Target-compatible workspace spdlog target unavailable; octaryn_native_logging will use stdio fallback for this configure.")
endif()

octaryn_add_dependency_wrapper(octaryn_native_mimalloc octaryn::deps::mimalloc)
octaryn_fetch_source_dependency(
    mimalloc
    GITHUB_REPOSITORY microsoft/mimalloc
    GIT_TAG v3.3.1
    OPTIONS
        "MI_BUILD_TESTS OFF"
        "MI_BUILD_SHARED OFF")
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
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG v0.13.1
    OPTIONS
        "TRACY_ENABLE ON"
        "TRACY_ON_DEMAND ON"
        "TRACY_NO_CALLSTACK ON"
        "TRACY_NO_SAMPLING ON"
        "TRACY_NO_SYSTEM_TRACING ON"
        "TRACY_NO_FRAME_IMAGE ON")
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
    GITHUB_REPOSITORY taskflow/taskflow
    GIT_TAG v4.0.0)
if(taskflow_source_dir)
    octaryn_add_header_only_dependency(octaryn_native_taskflow "${taskflow_source_dir}")
endif()

# Physics backend: Erin Catto's Box3D (portable C17, no dependencies).
octaryn_add_dependency_wrapper(octaryn_native_box3d octaryn::deps::box3d)
octaryn_fetch_source_dependency(
    box3d
    GITHUB_REPOSITORY erincatto/box3d
    GIT_TAG v0.1.0
    OPTIONS
        "BOX3D_BUILD_SAMPLES OFF"
        "BOX3D_BUILD_TESTS OFF"
        "BUILD_SHARED_LIBS OFF"
        "CMAKE_POSITION_INDEPENDENT_CODE ON")
octaryn_link_first_available_dependency(octaryn_native_box3d box3d_available box3d Box3D::Box3D)
if(box3d_available)
    set(OCTARYN_NATIVE_BOX3D_AVAILABLE ON)
endif()
