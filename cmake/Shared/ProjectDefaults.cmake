include_guard(GLOBAL)

include(CheckIPOSupported)

option(OCTARYN_ENABLE_LTO "Enable link time optimization outside Debug builds." OFF)

function(octaryn_setup_project_defaults)
    set(CMAKE_C_STANDARD 17 PARENT_SCOPE)
    set(CMAKE_C_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)

    find_program(OCTARYN_SCCACHE_PROGRAM sccache)
    if(OCTARYN_SCCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${OCTARYN_SCCACHE_PROGRAM}" PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${OCTARYN_SCCACHE_PROGRAM}" PARENT_SCOPE)
    endif()

    if(OCTARYN_ENABLE_LTO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        check_ipo_supported(RESULT octaryn_ipo_supported OUTPUT octaryn_ipo_output)
        if(octaryn_ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON PARENT_SCOPE)
        else()
            message(WARNING "IPO/LTO requested but unavailable: ${octaryn_ipo_output}")
        endif()
    endif()
endfunction()
