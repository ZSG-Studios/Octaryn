include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

# SDL_ttf already provides the workspace-built FreeType target.
if(NOT TARGET Freetype::Freetype)
    message(FATAL_ERROR "RmlUi requires the client FreeType dependency.")
endif()

octaryn_add_dependency_wrapper(octaryn_client_rmlui octaryn::deps::rmlui)
octaryn_fetch_source_dependency(
    RmlUi
    URL https://github.com/mikke89/RmlUi/archive/refs/tags/6.2.tar.gz
    URL_HASH SHA256=814c3ff7b9666280338d8f0dda85979f5daf028d01c85fc8975431d1e2fd8e8b
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "RMLUI_FONT_ENGINE freetype"
        "RMLUI_SAMPLES OFF"
        "RMLUI_TESTS OFF"
        "RMLUI_LUA_BINDINGS OFF"
        "RMLUI_LOTTIE_PLUGIN OFF"
        "RMLUI_SVG_PLUGIN OFF"
        "RMLUI_COMPILER_OPTIONS OFF")
if(NOT TARGET RmlUi::Core)
    message(FATAL_ERROR "RmlUi 6.2 could not be built from workspace sources.")
endif()
target_link_libraries(octaryn_client_rmlui INTERFACE RmlUi::Core)
set_target_properties(rmlui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(OCTARYN_RMLUI_SOURCE_DIR "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/rmlui")
add_library(octaryn_third_party_rmlui_sdl STATIC
    "${OCTARYN_RMLUI_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp")
target_include_directories(octaryn_third_party_rmlui_sdl SYSTEM PUBLIC
    "${OCTARYN_RMLUI_SOURCE_DIR}/Backends")
target_compile_definitions(octaryn_third_party_rmlui_sdl PUBLIC RMLUI_SDL_VERSION_MAJOR=3)
target_link_libraries(octaryn_third_party_rmlui_sdl PUBLIC RmlUi::Core octaryn::deps::sdl3)
set_target_properties(octaryn_third_party_rmlui_sdl PROPERTIES POSITION_INDEPENDENT_CODE ON)
octaryn_add_dependency_wrapper(octaryn_client_rmlui_sdl octaryn::deps::rmlui_sdl)
target_link_libraries(octaryn_client_rmlui_sdl INTERFACE octaryn_third_party_rmlui_sdl)

foreach(rmlui_target IN ITEMS rmlui_core rmlui_debugger octaryn_third_party_rmlui_sdl)
    if(TARGET ${rmlui_target})
        set_target_properties(${rmlui_target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${OCTARYN_DEPENDENCY_BUILD_ROOT}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${OCTARYN_DEPENDENCY_BUILD_ROOT}/lib"
            RUNTIME_OUTPUT_DIRECTORY "${OCTARYN_DEPENDENCY_BUILD_ROOT}/bin")
    endif()
endforeach()
