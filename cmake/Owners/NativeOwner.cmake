include_guard(GLOBAL)

function(octaryn_add_native_owner target_name)
    add_custom_target(${target_name})
endfunction()

function(octaryn_add_native_static_library target_name owner)
    cmake_parse_arguments(OCTARYN_NATIVE "" "" "SOURCES;PUBLIC_INCLUDE_DIRS;PRIVATE_LINKS" ${ARGN})

    if(NOT OCTARYN_NATIVE_SOURCES)
        message(FATAL_ERROR "Native target ${target_name} must declare explicit source files.")
    endif()

    add_library(${target_name} STATIC ${OCTARYN_NATIVE_SOURCES})
    octaryn_apply_owner_layout(${target_name} "${owner}")
    octaryn_enable_default_warnings(${target_name})

    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON)

    if(OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS)
        target_include_directories(${target_name}
            PUBLIC
                ${OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS})
    endif()

    if(OCTARYN_NATIVE_PRIVATE_LINKS)
        target_link_libraries(${target_name}
            PRIVATE
                ${OCTARYN_NATIVE_PRIVATE_LINKS})
    endif()
endfunction()

function(octaryn_add_native_shared_library target_name owner)
    cmake_parse_arguments(OCTARYN_NATIVE "" "" "SOURCES;PUBLIC_INCLUDE_DIRS;PRIVATE_LINKS" ${ARGN})

    if(NOT OCTARYN_NATIVE_SOURCES)
        message(FATAL_ERROR "Native target ${target_name} must declare explicit source files.")
    endif()

    add_library(${target_name} SHARED ${OCTARYN_NATIVE_SOURCES})
    octaryn_apply_owner_layout(${target_name} "${owner}")
    octaryn_enable_default_warnings(${target_name})

    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON)

    if(OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS)
        target_include_directories(${target_name}
            PUBLIC
                ${OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS})
    endif()

    if(OCTARYN_NATIVE_PRIVATE_LINKS)
        target_link_libraries(${target_name}
            PRIVATE
                ${OCTARYN_NATIVE_PRIVATE_LINKS})
    endif()
endfunction()

function(octaryn_add_native_executable target_name owner)
    cmake_parse_arguments(OCTARYN_NATIVE "" "" "SOURCES;PUBLIC_INCLUDE_DIRS;PRIVATE_LINKS" ${ARGN})

    if(NOT OCTARYN_NATIVE_SOURCES)
        message(FATAL_ERROR "Native target ${target_name} must declare explicit source files.")
    endif()

    add_executable(${target_name} ${OCTARYN_NATIVE_SOURCES})
    octaryn_apply_owner_layout(${target_name} "${owner}")
    octaryn_enable_default_warnings(${target_name})

    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    if(OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS)
        target_include_directories(${target_name}
            PUBLIC
                ${OCTARYN_NATIVE_PUBLIC_INCLUDE_DIRS})
    endif()

    if(OCTARYN_NATIVE_PRIVATE_LINKS)
        target_link_libraries(${target_name}
            PRIVATE
                ${OCTARYN_NATIVE_PRIVATE_LINKS})
    endif()
endfunction()
