include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

set(OCTARYN_CLIENT_SDL3_AVAILABLE OFF)

octaryn_add_dependency_wrapper(octaryn_client_sdl3 octaryn::deps::sdl3)
octaryn_fetch_source_dependency(
    SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.4
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF")
if(TARGET SDL3::SDL3)
    target_link_libraries(octaryn_client_sdl3 INTERFACE SDL3::SDL3)
    set(OCTARYN_CLIENT_SDL3_AVAILABLE ON)
elseif(TARGET SDL3::SDL3-static)
    target_link_libraries(octaryn_client_sdl3 INTERFACE SDL3::SDL3-static)
    set(OCTARYN_CLIENT_SDL3_AVAILABLE ON)
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
        target_link_libraries(octaryn_client_openal INTERFACE OpenAL::OpenAL)
    elseif(TARGET OpenAL)
        target_link_libraries(octaryn_client_openal INTERFACE OpenAL)
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

if(NOT TARGET octaryn::deps::sdl3_image)
    octaryn_add_dependency_wrapper(octaryn_client_sdl3_image octaryn::deps::sdl3_image)
    octaryn_fetch_source_dependency(
        SDL3_image
        URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.2/SDL3_image-3.4.2.tar.gz
        URL_HASH SHA256=82fdb88cf1a9cbdc1c77797aaa3292e6d22ce12586be718c8ea43530df1536b4
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "SDLIMAGE_AVIF OFF"
            "SDLIMAGE_BACKEND_IMAGEIO OFF"
            "SDLIMAGE_DEPS_SHARED OFF"
            "SDLIMAGE_INSTALL OFF"
            "SDLIMAGE_JPG OFF"
            "SDLIMAGE_JXL OFF"
            "SDLIMAGE_PNG OFF"
            "SDLIMAGE_SAMPLES OFF"
            "SDLIMAGE_TESTS OFF"
            "SDLIMAGE_TIF OFF"
            "SDLIMAGE_VENDORED ON"
            "SDLIMAGE_WEBP OFF")
    octaryn_link_first_available_dependency(octaryn_client_sdl3_image sdl3_image_available SDL3_image::SDL3_image SDL3_image::SDL3_image-static)
endif()

if(NOT TARGET octaryn::deps::sdl3_ttf)
    octaryn_add_dependency_wrapper(octaryn_client_sdl3_ttf octaryn::deps::sdl3_ttf)
    octaryn_fetch_source_dependency(
        SDL3_ttf
        GITHUB_REPOSITORY libsdl-org/SDL_ttf
        GIT_TAG release-3.2.2
        GIT_SUBMODULES external/freetype
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "SDLTTF_HARFBUZZ OFF"
            "SDLTTF_INSTALL OFF"
            "SDLTTF_PLUTOSVG OFF"
            "SDLTTF_SAMPLES OFF"
            "SDLTTF_TESTS OFF"
            "SDLTTF_VENDORED ON")
    octaryn_link_first_available_dependency(octaryn_client_sdl3_ttf sdl3_ttf_available SDL3_ttf::SDL3_ttf SDL3_ttf::SDL3_ttf-static)
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
endif()

if(NOT TARGET octaryn::deps::imgui)
    octaryn_add_dependency_wrapper(octaryn_client_imgui octaryn::deps::imgui)
    octaryn_fetch_header_dependency(
        imgui
        imgui_source_dir
        GITHUB_REPOSITORY pthom/imgui
        GIT_TAG 285b38e2a7cfb2850ef27385f4e70df0f74f6b97)
    set(OCTARYN_IMGUI_SOURCE_DIR "${imgui_source_dir}" CACHE INTERNAL "Fetched Dear ImGui source dir" FORCE)
    if(imgui_source_dir AND NOT TARGET octaryn_third_party_imgui)
        file(GLOB imgui_sources CONFIGURE_DEPENDS
            "${imgui_source_dir}/*.h"
            "${imgui_source_dir}/*.cpp"
            "${imgui_source_dir}/misc/cpp/*.h"
            "${imgui_source_dir}/misc/cpp/*.cpp")
        add_library(octaryn_third_party_imgui STATIC ${imgui_sources})
        target_include_directories(octaryn_third_party_imgui
            PUBLIC
                "${imgui_source_dir}"
                "${imgui_source_dir}/backends"
                "${imgui_source_dir}/misc/cpp")
        set_target_properties(octaryn_third_party_imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_imgui imgui_available octaryn_third_party_imgui)
endif()

if(NOT TARGET octaryn::deps::implot)
    octaryn_add_dependency_wrapper(octaryn_client_implot octaryn::deps::implot)
    octaryn_fetch_header_dependency(
        implot
        implot_source_dir
        GITHUB_REPOSITORY pthom/implot
        GIT_TAG e6c36daf587b5eafebb533af1826b6d114b45421)
    if(implot_source_dir AND NOT TARGET octaryn_third_party_implot)
        add_library(octaryn_third_party_implot STATIC
            "${implot_source_dir}/implot.cpp"
            "${implot_source_dir}/implot_demo.cpp"
            "${implot_source_dir}/implot_items.cpp")
        target_include_directories(octaryn_third_party_implot PUBLIC "${implot_source_dir}")
        target_link_libraries(octaryn_third_party_implot PUBLIC octaryn_third_party_imgui)
        target_compile_definitions(octaryn_third_party_implot PRIVATE "IMPLOT_CUSTOM_NUMERIC_TYPES=(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned long)(signed long long)(unsigned long long)(float)(double)(long double)")
        set_target_properties(octaryn_third_party_implot PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_implot implot_available octaryn_third_party_implot)
endif()

if(NOT TARGET octaryn::deps::implot3d)
    octaryn_add_dependency_wrapper(octaryn_client_implot3d octaryn::deps::implot3d)
    octaryn_fetch_header_dependency(
        implot3d
        implot3d_source_dir
        GITHUB_REPOSITORY pthom/implot3d
        GIT_TAG eb4ccd75f34b07646dfefb13b14f2df728bfd7ca)
    if(implot3d_source_dir AND NOT TARGET octaryn_third_party_implot3d)
        add_library(octaryn_third_party_implot3d STATIC
            "${implot3d_source_dir}/implot3d.cpp"
            "${implot3d_source_dir}/implot3d_demo.cpp"
            "${implot3d_source_dir}/implot3d_items.cpp"
            "${implot3d_source_dir}/implot3d_meshes.cpp")
        target_include_directories(octaryn_third_party_implot3d PUBLIC "${implot3d_source_dir}")
        target_link_libraries(octaryn_third_party_implot3d PUBLIC octaryn_third_party_imgui octaryn_third_party_implot)
        target_compile_definitions(octaryn_third_party_implot3d PRIVATE "IMPLOT3D_CUSTOM_NUMERIC_TYPES=(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned long)(signed long long)(unsigned long long)(float)(double)(long double)")
        set_target_properties(octaryn_third_party_implot3d PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_implot3d implot3d_available octaryn_third_party_implot3d)
endif()

if(NOT TARGET octaryn::deps::imgui_node_editor)
    octaryn_add_dependency_wrapper(octaryn_client_imgui_node_editor octaryn::deps::imgui_node_editor)
    octaryn_fetch_header_dependency(
        imgui_node_editor
        imgui_node_editor_source_dir
        GITHUB_REPOSITORY pthom/imgui-node-editor
        GIT_TAG 432c515535f4755c89235d58e71343c7c62ed317)
    if(imgui_node_editor_source_dir AND NOT TARGET octaryn_third_party_imgui_node_editor)
        file(GLOB imgui_node_editor_sources CONFIGURE_DEPENDS
            "${imgui_node_editor_source_dir}/*.cpp"
            "${imgui_node_editor_source_dir}/*.h")
        add_library(octaryn_third_party_imgui_node_editor STATIC ${imgui_node_editor_sources})
        target_include_directories(octaryn_third_party_imgui_node_editor PUBLIC "${imgui_node_editor_source_dir}")
        target_link_libraries(octaryn_third_party_imgui_node_editor PUBLIC octaryn_third_party_imgui)
        target_compile_options(octaryn_third_party_imgui_node_editor PRIVATE "-include" "exception")
        set_target_properties(octaryn_third_party_imgui_node_editor PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_imgui_node_editor imgui_node_editor_available octaryn_third_party_imgui_node_editor)
endif()

if(NOT TARGET octaryn::deps::imguizmo)
    octaryn_add_dependency_wrapper(octaryn_client_imguizmo octaryn::deps::imguizmo)
    octaryn_fetch_header_dependency(
        imguizmo
        imguizmo_source_dir
        GITHUB_REPOSITORY pthom/ImGuizmo
        GIT_TAG bbf06a1b0a1f18668acc6687ae283d6a12368271)
    if(imguizmo_source_dir AND NOT TARGET octaryn_third_party_imguizmo)
        add_library(octaryn_third_party_imguizmo STATIC
            "${imguizmo_source_dir}/ImGuizmo.cpp"
            "${imguizmo_source_dir}/GraphEditor.cpp"
            "${imguizmo_source_dir}/ImCurveEdit.cpp"
            "${imguizmo_source_dir}/ImGradient.cpp"
            "${imguizmo_source_dir}/ImSequencer.cpp")
        target_include_directories(octaryn_third_party_imguizmo PUBLIC "${imguizmo_source_dir}")
        target_link_libraries(octaryn_third_party_imguizmo PUBLIC octaryn_third_party_imgui)
        set_target_properties(octaryn_third_party_imguizmo PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_imguizmo imguizmo_available octaryn_third_party_imguizmo)
endif()

if(NOT TARGET octaryn::deps::imanim)
    octaryn_add_dependency_wrapper(octaryn_client_imanim octaryn::deps::imanim)
    octaryn_fetch_header_dependency(
        imanim
        imanim_source_dir
        GITHUB_REPOSITORY pthom/ImAnim
        GIT_TAG 51b78e795cf4d64f7d016d148b46a02e837e4023)
    if(imanim_source_dir AND NOT TARGET octaryn_third_party_imanim)
        add_library(octaryn_third_party_imanim STATIC
            "${imanim_source_dir}/im_anim.cpp"
            "${imanim_source_dir}/im_anim_demo_basics.cpp"
            "${imanim_source_dir}/im_anim_demo.cpp"
            "${imanim_source_dir}/im_anim_doc.cpp"
            "${imanim_source_dir}/im_anim_usecase.cpp")
        target_include_directories(octaryn_third_party_imanim PUBLIC "${imanim_source_dir}")
        target_link_libraries(octaryn_third_party_imanim PUBLIC octaryn_third_party_imgui)
        set_target_properties(octaryn_third_party_imanim PROPERTIES POSITION_INDEPENDENT_CODE ON)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(octaryn_third_party_imanim PRIVATE -Wno-unknown-warning-option -Wno-nontrivial-memaccess -Wno-nontrivial-memcall)
        endif()
    endif()
    octaryn_link_first_available_dependency(octaryn_client_imanim imanim_available octaryn_third_party_imanim)
endif()

if(NOT TARGET octaryn::deps::imfiledialog)
    octaryn_add_dependency_wrapper(octaryn_client_imfiledialog octaryn::deps::imfiledialog)
    octaryn_fetch_header_dependency(
        imfiledialog
        imfiledialog_source_dir
        GITHUB_REPOSITORY pthom/ImFileDialog
        GIT_TAG c9819dd90450262efe7682839bb751c38173e1d8)
    set(imgui_shim_include_dir "${OCTARYN_DEPENDENCY_BUILD_ROOT}/generated/imgui_shim")
    set(stb_image_shim_include_dir "${OCTARYN_DEPENDENCY_BUILD_ROOT}/generated/stb_image_shim")
    file(MAKE_DIRECTORY "${imgui_shim_include_dir}/imgui")
    file(MAKE_DIRECTORY "${stb_image_shim_include_dir}")
    file(WRITE "${imgui_shim_include_dir}/imgui/imgui.h" "#pragma once\n#include <imgui.h>\n")
    file(WRITE "${imgui_shim_include_dir}/imgui/imgui_internal.h" "#pragma once\n#include <imgui_internal.h>\n")
    if(imguizmo_source_dir AND EXISTS "${imguizmo_source_dir}/example/stb_image.h")
        file(COPY_FILE "${imguizmo_source_dir}/example/stb_image.h" "${stb_image_shim_include_dir}/stb_image.h" ONLY_IF_DIFFERENT)
    endif()
    if(imfiledialog_source_dir AND NOT TARGET octaryn_third_party_imfiledialog)
        add_library(octaryn_third_party_imfiledialog STATIC
            "${imfiledialog_source_dir}/ImFileDialog.cpp")
        target_include_directories(octaryn_third_party_imfiledialog
            PUBLIC
                "${imfiledialog_source_dir}"
            PRIVATE
                "${imgui_shim_include_dir}"
                "${stb_image_shim_include_dir}")
        target_link_libraries(octaryn_third_party_imfiledialog PUBLIC octaryn_third_party_imgui octaryn::deps::sdl3_image)
        set_target_properties(octaryn_third_party_imfiledialog PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    endif()
    octaryn_link_first_available_dependency(octaryn_client_imfiledialog imfiledialog_available octaryn_third_party_imfiledialog)
endif()

if(NOT TARGET octaryn::deps::ozz_animation)
    octaryn_add_dependency_wrapper(octaryn_client_ozz_animation octaryn::deps::ozz_animation)
    octaryn_fetch_source_dependency(
        ozz_animation
        GITHUB_REPOSITORY guillaumeblanc/ozz-animation
        GIT_TAG 0.16.0
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "ozz_build_tools OFF"
            "ozz_build_fbx OFF"
            "ozz_build_gltf OFF"
            "ozz_build_data OFF"
            "ozz_build_samples OFF"
            "ozz_build_howtos OFF"
            "ozz_build_tests OFF"
            "ozz_build_postfix OFF")
    octaryn_link_first_available_dependency(octaryn_client_ozz_animation ozz_animation_available ozz_animation)
    if(TARGET ozz_animation_offline)
        target_link_libraries(octaryn_client_ozz_animation INTERFACE ozz_animation_offline)
    endif()
    if(TARGET ozz_geometry)
        target_link_libraries(octaryn_client_ozz_animation INTERFACE ozz_geometry)
    endif()
endif()
