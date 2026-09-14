# Upstream's Clang -Wall becomes clang-cl /Wall (all compatibility warnings).
# Forward it to the Clang driver explicitly without modifying pinned sources.
function(octaryn_slang_rhi_compiler_flags)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        get_target_property(warnings slang-rhi-warnings INTERFACE_COMPILE_OPTIONS)
        list(TRANSFORM warnings REPLACE "-Wall" "-clang:-Wall")
        set_property(TARGET slang-rhi-warnings PROPERTY INTERFACE_COMPILE_OPTIONS "${warnings}")
    endif()
endfunction()
cmake_language(DEFER CALL octaryn_slang_rhi_compiler_flags)
