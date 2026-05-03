include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

function(octaryn_link_spirv_cross_targets wrapper_target)
    foreach(octaryn_spirv_cross_target
        spirv-cross-core
        spirv-cross-reflect
        spirv-cross-msl
        SPIRV-Cross::spirv-cross-core
        SPIRV-Cross::spirv-cross-reflect
        SPIRV-Cross::spirv-cross-msl)
        if(TARGET "${octaryn_spirv_cross_target}")
            target_link_libraries("${wrapper_target}" INTERFACE "${octaryn_spirv_cross_target}")
        endif()
    endforeach()
endfunction()

function(octaryn_require_spirv_cross)
    if(TARGET octaryn::deps::spirv_cross)
        return()
    endif()

    octaryn_add_dependency_wrapper(octaryn_tool_spirv_cross octaryn::deps::spirv_cross)
    octaryn_fetch_source_dependency(
        SPIRV-Cross
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
        GIT_TAG vulkan-sdk-1.4.341.0
        OPTIONS
            "SPIRV_CROSS_CLI OFF"
            "SPIRV_CROSS_ENABLE_TESTS OFF"
            "SPIRV_CROSS_ENABLE_GLSL ON"
            "SPIRV_CROSS_ENABLE_HLSL ON"
            "SPIRV_CROSS_ENABLE_MSL ON"
            "SPIRV_CROSS_ENABLE_REFLECT ON"
            "SPIRV_CROSS_ENABLE_CPP OFF")
    octaryn_link_spirv_cross_targets(octaryn_tool_spirv_cross)
    if(TARGET spirv-cross-c AND NOT TARGET spirv_cross_c)
        add_library(spirv_cross_c ALIAS spirv-cross-c)
    endif()
endfunction()

if(NOT TARGET octaryn::deps::glaze)
    octaryn_add_dependency_wrapper(octaryn_tool_glaze octaryn::deps::glaze)
    octaryn_fetch_source_dependency(
        glaze
        GITHUB_REPOSITORY stephenberry/glaze
        GIT_TAG v7.4.0
        OPTIONS
            "glaze_BUILD_TESTS OFF")
    if(TARGET glaze::glaze)
        target_link_libraries(octaryn_tool_glaze INTERFACE glaze::glaze)
    endif()
endif()

if(NOT TARGET octaryn::deps::sdl3)
    octaryn_add_dependency_wrapper(octaryn_tool_sdl3 octaryn::deps::sdl3)
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
        target_link_libraries(octaryn_tool_sdl3 INTERFACE SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-static)
        target_link_libraries(octaryn_tool_sdl3 INTERFACE SDL3::SDL3-static)
    endif()
endif()

if(NOT TARGET octaryn::deps::shadercross)
    octaryn_add_dependency_wrapper(octaryn_tool_shadercross octaryn::deps::shadercross)
    octaryn_require_spirv_cross()
    octaryn_fetch_source_dependency(
        SDL3_shadercross
        GITHUB_REPOSITORY libsdl-org/SDL_shadercross
        GIT_TAG 6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "SDLSHADERCROSS_CLI OFF"
            "SDLSHADERCROSS_DXC OFF"
            "SDLSHADERCROSS_INSTALL OFF"
            "SDLSHADERCROSS_SHARED OFF"
            "SDLSHADERCROSS_SPIRVCROSS_SHARED OFF"
            "SDLSHADERCROSS_STATIC ON")
    if(TARGET SDL3_shadercross::SDL3_shadercross)
        target_link_libraries(octaryn_tool_shadercross INTERFACE SDL3_shadercross::SDL3_shadercross)
    elseif(TARGET SDL3_shadercross-static)
        target_link_libraries(octaryn_tool_shadercross INTERFACE SDL3_shadercross-static)
    endif()
endif()

if(NOT TARGET octaryn::deps::shaderc)
    octaryn_add_dependency_wrapper(octaryn_tool_shaderc octaryn::deps::shaderc)
    octaryn_fetch_header_dependency(
        SPIRV-Headers
        spirv_headers_source_dir
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
        GIT_TAG vulkan-sdk-1.4.341.0)
    octaryn_fetch_header_dependency(
        SPIRV-Tools
        spirv_tools_source_dir
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
        GIT_TAG vulkan-sdk-1.4.341.0)
    octaryn_fetch_header_dependency(
        glslang
        glslang_source_dir
        GITHUB_REPOSITORY KhronosGroup/glslang
        GIT_TAG vulkan-sdk-1.4.341.0)
    octaryn_fetch_source_dependency(
        shaderc
        FORCE_GIT
        GITHUB_REPOSITORY google/shaderc
        GIT_TAG v2026.2
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "SHADERC_SKIP_TESTS ON"
            "SHADERC_SKIP_EXAMPLES ON"
            "SHADERC_SKIP_COPYRIGHT_CHECK ON"
            "SHADERC_SKIP_INSTALL ON"
            "SHADERC_SPIRV_HEADERS_DIR ${spirv_headers_source_dir}"
            "SHADERC_SPIRV_TOOLS_DIR ${spirv_tools_source_dir}"
            "SHADERC_GLSLANG_DIR ${glslang_source_dir}")
    target_include_directories(octaryn_tool_shaderc
        INTERFACE
            "${OCTARYN_SOURCE_DEPENDENCY_SOURCE_ROOT}/shaderc/libshaderc/include")
    if(TARGET shaderc_combined)
        target_link_libraries(octaryn_tool_shaderc INTERFACE shaderc_combined)
    elseif(TARGET shaderc_shared)
        target_link_libraries(octaryn_tool_shaderc INTERFACE shaderc_shared)
    elseif(TARGET shaderc)
        target_link_libraries(octaryn_tool_shaderc INTERFACE shaderc)
    endif()
endif()

if(NOT TARGET octaryn::deps::spirv_tools)
    octaryn_add_dependency_wrapper(octaryn_tool_spirv_tools octaryn::deps::spirv_tools)
    if(NOT TARGET SPIRV-Tools)
        octaryn_fetch_source_dependency(
            SPIRV-Tools
            GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
            GIT_TAG vulkan-sdk-1.4.341.0
            OPTIONS
                "SPIRV_SKIP_TESTS ON"
                "SPIRV_SKIP_EXECUTABLES ON"
                "SPIRV_TOOLS_BUILD_STATIC ON")
    endif()
    if(TARGET SPIRV-Tools)
        target_link_libraries(octaryn_tool_spirv_tools INTERFACE SPIRV-Tools)
    elseif(TARGET SPIRV-Tools-static)
        target_link_libraries(octaryn_tool_spirv_tools INTERFACE SPIRV-Tools-static)
    elseif(TARGET SPIRV-Tools-opt)
        target_link_libraries(octaryn_tool_spirv_tools INTERFACE SPIRV-Tools-opt)
    endif()
endif()

octaryn_require_spirv_cross()

if(NOT TARGET octaryn::deps::fastgltf)
    octaryn_add_dependency_wrapper(octaryn_tool_fastgltf octaryn::deps::fastgltf)
    octaryn_fetch_source_dependency(
        fastgltf
        GITHUB_REPOSITORY spnda/fastgltf
        GIT_TAG v0.9.0
        OPTIONS
            "FASTGLTF_DOWNLOAD_SIMDJSON OFF"
            "FASTGLTF_TESTS OFF")
    octaryn_link_first_available_dependency(octaryn_tool_fastgltf fastgltf_available fastgltf::fastgltf)
endif()

if(NOT TARGET octaryn::deps::ktx)
    set(octaryn_ktx_astc_options
        "ASTCENC_INVARIANCE OFF")
    if(OCTARYN_TARGET_ARCH STREQUAL "arm64")
        list(APPEND octaryn_ktx_astc_options
            "ASTCENC_ISA_NONE ON"
            "ASTCENC_ISA_AVX2 OFF"
            "ASTCENC_ISA_SSE41 OFF"
            "ASTCENC_ISA_SSE2 OFF"
            "ASTCENC_ISA_NEON OFF"
            "ASTCENC_X86_GATHERS OFF"
            "BASISU_SUPPORT_SSE OFF")
    endif()

    octaryn_add_dependency_wrapper(octaryn_tool_ktx octaryn::deps::ktx)
    octaryn_fetch_source_dependency(
        KTX-Software
        FORCE_GIT
        NO_GIT_SUBMODULES
        GITHUB_REPOSITORY KhronosGroup/KTX-Software
        GIT_TAG v4.4.2
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "KTX_FEATURE_TESTS OFF"
            "KTX_FEATURE_TOOLS OFF"
            "KTX_FEATURE_LOADTEST_APPS OFF"
            "KTX_FEATURE_GL_UPLOAD OFF"
            ${octaryn_ktx_astc_options})
    octaryn_link_first_available_dependency(octaryn_tool_ktx ktx_available KTX::ktx)
endif()

if(NOT TARGET octaryn::deps::meshoptimizer)
    octaryn_add_dependency_wrapper(octaryn_tool_meshoptimizer octaryn::deps::meshoptimizer)
    octaryn_fetch_source_dependency(
        meshoptimizer
        GITHUB_REPOSITORY zeux/meshoptimizer
        GIT_TAG v1.1.1)
    octaryn_link_first_available_dependency(octaryn_tool_meshoptimizer meshoptimizer_available meshoptimizer)
endif()
