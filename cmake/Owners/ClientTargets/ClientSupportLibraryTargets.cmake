# Native support libraries relocated from the retired shared owner: logging,
# memory, profiling and the job scheduler the renderer boots on.
octaryn_add_native_static_library(
    octaryn_native_logging
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeLogging/octaryn_native_log.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeLogging"
    PRIVATE_LINKS
        octaryn::deps::spdlog)

if(OCTARYN_NATIVE_SPDLOG_AVAILABLE)
    target_compile_definitions(octaryn_native_logging
        PRIVATE
            OCTARYN_NATIVE_LOGGING_USE_SPDLOG)
endif()

octaryn_add_native_static_library(
    octaryn_native_memory
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeMemory/NativeMemory/octaryn_native_memory.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeMemory/NativeMemory"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeLogging"
    PRIVATE_LINKS
        octaryn_native_logging
        octaryn::deps::mimalloc)

if(OCTARYN_NATIVE_MIMALLOC_AVAILABLE)
    target_compile_definitions(octaryn_native_memory
        PRIVATE
            OCTARYN_NATIVE_MEMORY_USE_MIMALLOC)
endif()

octaryn_add_native_static_library(
    octaryn_native_profiling
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeProfiling/octaryn_native_profile.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeProfiling"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeDiagnostics/NativeLogging"
    PRIVATE_LINKS
        octaryn_native_logging)

target_link_libraries(octaryn_native_profiling
    PUBLIC
        octaryn::deps::tracy)

if(OCTARYN_NATIVE_TRACY_AVAILABLE)
    target_compile_definitions(octaryn_native_profiling
        PUBLIC
            OCTARYN_NATIVE_PROFILING_USE_TRACY)
endif()

octaryn_add_native_shared_library(
    octaryn_native_jobs
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeJobs/octaryn_native_schedule_runtime.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeJobs/octaryn_native_schedule_policy.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeJobs/octaryn_native_worker_policy.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Libraries/NativeJobs"
    PRIVATE_LINKS
        octaryn_native_logging
        octaryn_native_profiling
        octaryn::native_threads
        octaryn::deps::taskflow)

add_dependencies(octaryn_client_native
    octaryn_native_logging
    octaryn_native_memory
    octaryn_native_profiling
    octaryn_native_jobs)
