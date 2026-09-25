include_guard(GLOBAL)

set(OCTARYN_TARGET_ARCH "$ENV{OCTARYN_TARGET_ARCH}" CACHE STRING "Target CPU architecture: x64 or arm64.")
if(NOT OCTARYN_TARGET_ARCH)
    set(OCTARYN_TARGET_ARCH "x64" CACHE STRING "Target CPU architecture: x64 or arm64." FORCE)
endif()
set_property(CACHE OCTARYN_TARGET_ARCH PROPERTY STRINGS x64 arm64)

if(NOT OCTARYN_TARGET_ARCH STREQUAL "x64" AND NOT OCTARYN_TARGET_ARCH STREQUAL "arm64")
    message(FATAL_ERROR "Unsupported OCTARYN_TARGET_ARCH=${OCTARYN_TARGET_ARCH}; expected x64 or arm64.")
endif()

function(octaryn_arch_select output_var x64_value arm64_value)
    if(OCTARYN_TARGET_ARCH STREQUAL "arm64")
        set(${output_var} "${arm64_value}" PARENT_SCOPE)
    else()
        set(${output_var} "${x64_value}" PARENT_SCOPE)
    endif()
endfunction()
