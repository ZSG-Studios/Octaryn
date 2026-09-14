include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

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
