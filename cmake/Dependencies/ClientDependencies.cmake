include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

set(OCTARYN_CLIENT_SDL3_AVAILABLE OFF)


include(Dependencies/SlangSdk)

octaryn_add_dependency_wrapper(octaryn_client_sdl3 octaryn::deps::sdl3)
octaryn_fetch_source_dependency(
    SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.4
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_GPU OFF"
        "SDL_RENDER OFF"
        "SDL_RENDER_GPU OFF"
        "SDL_OPENGL OFF"
        "SDL_OPENGLES OFF"
        "SDL_KMSDRM OFF"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF")
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
    set(octaryn_openal_options
        "ALSOFT_UTILS OFF"
        "ALSOFT_EXAMPLES OFF"
        "ALSOFT_TESTS OFF"
        "LIBTYPE STATIC")
    if(WIN32)
        list(APPEND octaryn_openal_options
            "ALSOFT_BACKEND_ALSA OFF"
            "ALSOFT_BACKEND_JACK OFF"
            "ALSOFT_BACKEND_PIPEWIRE OFF"
            "ALSOFT_BACKEND_PULSEAUDIO OFF"
            "ALSOFT_BACKEND_SNDIO OFF")
    endif()
    octaryn_fetch_source_dependency(
        OpenALSoft
        GITHUB_REPOSITORY kcat/openal-soft
        GIT_TAG 1.25.1
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
        GITHUB_REPOSITORY mackron/miniaudio
        GIT_TAG 0.11.25)
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
        GITHUB_REPOSITORY stephenberry/glaze
        GIT_TAG v7.4.0
        OPTIONS
            "glaze_BUILD_TESTS OFF")
    if(TARGET glaze::glaze)
        target_link_libraries(octaryn_client_glaze INTERFACE glaze::glaze)
    endif()
endif()

if(NOT TARGET octaryn::deps::fastgltf)
    octaryn_add_dependency_wrapper(octaryn_client_fastgltf octaryn::deps::fastgltf)
    octaryn_fetch_source_dependency(
        fastgltf
        GITHUB_REPOSITORY spnda/fastgltf
        GIT_TAG v0.9.0
        OPTIONS
            "FASTGLTF_DOWNLOAD_SIMDJSON OFF"
            "FASTGLTF_TESTS OFF")
    octaryn_link_first_available_dependency(octaryn_client_fastgltf fastgltf_available fastgltf::fastgltf)
    if(TARGET fastgltf)
        get_target_property(fastgltf_includes fastgltf INTERFACE_INCLUDE_DIRECTORIES)
        if(fastgltf_includes)
            set_target_properties(fastgltf PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${fastgltf_includes}")
        endif()
    endif()
endif()

include(Dependencies/RmlUi)
