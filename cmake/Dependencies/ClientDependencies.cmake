include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

set(OCTARYN_CLIENT_SDL3_AVAILABLE OFF)


include(Dependencies/SlangSdk)

octaryn_add_dependency_wrapper(octaryn_client_sdl3 octaryn::deps::sdl3)
octaryn_fetch_source_dependency(
    SDL3
    GITHUB_REPOSITORY ${OCTARYN_DEP_sdl3_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_sdl3_TAG}
    OPTIONS ${OCTARYN_DEP_sdl3_OPTIONS})
if(TARGET SDL3::SDL3)
    target_link_libraries(octaryn_client_sdl3 INTERFACE SDL3::SDL3)
    set(OCTARYN_CLIENT_SDL3_AVAILABLE ON)
elseif(TARGET SDL3::SDL3-static)
    target_link_libraries(octaryn_client_sdl3 INTERFACE SDL3::SDL3-static)
    set(OCTARYN_CLIENT_SDL3_AVAILABLE ON)
endif()
if(NOT OCTARYN_CLIENT_SDL3_AVAILABLE)
    message(FATAL_ERROR "The active client requires the pinned SDL3 window/input library.")
endif()

if(NOT TARGET octaryn::deps::openal)
    octaryn_add_dependency_wrapper(octaryn_client_openal octaryn::deps::openal)
    set(octaryn_openal_options ${OCTARYN_DEP_openal_OPTIONS})
    if(WIN32)
        list(APPEND octaryn_openal_options
            # OpenAL 1.25 enables its gsl C++20 module wrapper for Clang 17+,
            # but CMake has no dependency scanner for clang-cl: explicit
            # CXX_MODULES file sets fail generate. The engine uses no modules.
            "ALSOFT_ENABLE_MODULES OFF"
            "ALSOFT_BACKEND_ALSA OFF"
            "ALSOFT_BACKEND_JACK OFF"
            "ALSOFT_BACKEND_PIPEWIRE OFF"
            "ALSOFT_BACKEND_PULSEAUDIO OFF"
            "ALSOFT_BACKEND_SNDIO OFF")
    endif()
    octaryn_fetch_source_dependency(
        OpenALSoft
        GITHUB_REPOSITORY ${OCTARYN_DEP_openal_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_openal_TAG}
        OPTIONS ${octaryn_openal_options})
    if(TARGET OpenAL::OpenAL)
        get_target_property(octaryn_openal_type OpenAL::OpenAL TYPE)
        target_link_libraries(octaryn_client_openal INTERFACE OpenAL::OpenAL)
    elseif(TARGET OpenAL)
        get_target_property(octaryn_openal_type OpenAL TYPE)
        target_link_libraries(octaryn_client_openal INTERFACE OpenAL)
    else()
        message(FATAL_ERROR "Action audio requires the pinned OpenAL Soft target.")
    endif()
    if(NOT octaryn_openal_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "Action audio requires static OpenAL Soft; shared runtime staging is not configured.")
    endif()
endif()

if(NOT TARGET octaryn::deps::miniaudio)
    octaryn_add_dependency_wrapper(octaryn_client_miniaudio octaryn::deps::miniaudio)
    octaryn_fetch_header_dependency(
        miniaudio
        miniaudio_source_dir
        GITHUB_REPOSITORY ${OCTARYN_DEP_miniaudio_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_miniaudio_TAG})
    if(miniaudio_source_dir)
        target_include_directories(octaryn_client_miniaudio SYSTEM INTERFACE "${miniaudio_source_dir}")
    else()
        message(FATAL_ERROR "Action audio requires the pinned miniaudio headers.")
    endif()
endif()

if(NOT TARGET octaryn::deps::glaze)
    octaryn_add_dependency_wrapper(octaryn_client_glaze octaryn::deps::glaze)
    octaryn_fetch_source_dependency(
        glaze
        GITHUB_REPOSITORY ${OCTARYN_DEP_glaze_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_glaze_TAG}
        OPTIONS ${OCTARYN_DEP_glaze_OPTIONS})
    if(TARGET glaze::glaze)
        target_link_libraries(octaryn_client_glaze INTERFACE glaze::glaze)
    endif()
endif()

include(Dependencies/GltfDependencies)

include(Dependencies/RmlUi)
