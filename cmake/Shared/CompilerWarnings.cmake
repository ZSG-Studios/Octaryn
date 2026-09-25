include_guard(GLOBAL)

function(octaryn_enable_default_warnings target_name)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${target_name} PRIVATE
                /clang:-Wall /clang:-Wextra /clang:-Wpedantic /clang:-Wconversion /clang:-Wshadow)
        else()
            target_compile_options(${target_name} PRIVATE /W4)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()
