include_guard(GLOBAL)

# Headers and optional diagnostic DLL come from the same verified archive.
# Runtime activation remains off unless an explicit diagnostic output is set.
function(octaryn_configure_gpu_perf_api target)
    if(NOT WIN32)
        return()
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/support/acquire_gpu_perf_api.py"
        --repo "${OCTARYN_WORKSPACE_ROOT_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    set(sdk "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/tools/gpu-perf-api/${OCTARYN_DEP_gpu_perf_api_TAG}/package/${OCTARYN_DEP_gpu_perf_api_SOURCE_SUBDIR}")
    target_include_directories(${target} SYSTEM PRIVATE "${sdk}/include")
    string(REPLACE "." ";" version "${OCTARYN_DEP_gpu_perf_api_TAG}")
    list(GET version 0 major)
    list(GET version 1 minor)
    # AMD release/project version is major.minor.update.build; GpaGetVersion
    # arguments are major,minor,build,update (the final two are reversed).
    list(GET version 2 update)
    list(GET version 3 build)
    target_compile_definitions(${target} PRIVATE
        OCTARYN_GPA_DLL="${sdk}/bin/GPUPerfAPIDX12-x64.dll"
        OCTARYN_GPA_MAJOR=${major} OCTARYN_GPA_MINOR=${minor}
        OCTARYN_GPA_BUILD=${build} OCTARYN_GPA_UPDATE=${update})
endfunction()
