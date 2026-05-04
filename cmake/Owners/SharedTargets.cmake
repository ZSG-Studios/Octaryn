include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)

octaryn_add_native_owner(octaryn_shared_native)

octaryn_add_native_static_library(
    octaryn_shared_host_abi
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi/octaryn_host_abi.c"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi")

target_compile_definitions(octaryn_shared_host_abi
    PRIVATE
        OCTARYN_ABI_BUILD)

octaryn_add_native_static_library(
    octaryn_native_logging
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeLogging/octaryn_native_log.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeLogging"
    PRIVATE_LINKS
        octaryn::deps::spdlog)

if(OCTARYN_NATIVE_SPDLOG_AVAILABLE)
    target_compile_definitions(octaryn_native_logging
        PRIVATE
            OCTARYN_NATIVE_LOGGING_USE_SPDLOG)
endif()

octaryn_add_native_static_library(
    octaryn_native_diagnostics
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics/octaryn_native_crash_diagnostics.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeCrashDiagnostics"
    PRIVATE_LINKS
        octaryn_native_logging
        octaryn::deps::cpptrace)

if(OCTARYN_NATIVE_CPPTRACE_AVAILABLE)
    target_compile_definitions(octaryn_native_diagnostics
        PRIVATE
            OCTARYN_NATIVE_DIAGNOSTICS_USE_CPPTRACE)
endif()

octaryn_add_native_static_library(
    octaryn_native_memory
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Memory/NativeMemory/octaryn_native_memory.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Memory/NativeMemory"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeLogging"
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
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeProfiling/octaryn_native_profile.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeProfiling"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Diagnostics/NativeLogging"
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

octaryn_add_native_static_library(
    octaryn_native_jobs
    shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs/octaryn_native_schedule_policy.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs/octaryn_native_worker_policy.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs"
    PRIVATE_LINKS
        octaryn_native_logging
        octaryn_native_profiling
        octaryn::native_threads
        octaryn::deps::taskflow)

add_dependencies(octaryn_shared_native
    octaryn_shared_host_abi
    octaryn_native_logging
    octaryn_native_diagnostics
    octaryn_native_memory
    octaryn_native_profiling
    octaryn_native_jobs)

octaryn_add_dotnet_owner(
    octaryn_shared
    shared
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Octaryn.Shared.csproj")

add_dependencies(octaryn_shared octaryn_shared_native)
